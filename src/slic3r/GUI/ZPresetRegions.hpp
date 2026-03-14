// ─── ORCA FullSpectrum: Z-Override Regions ───────────────────────────────────
// Each height zone stores only the PrintRegionConfig keys the user explicitly
// checked. No preset reference — values live directly in ModelConfig.
// ─────────────────────────────────────────────────────────────────────────────
#pragma once
#include <wx/wx.h>
#include <wx/grid.h>
#include <wx/dcbuffer.h>
#include <wx/scrolwin.h>
#include <map>
#include "libslic3r/Model.hpp"
#include "libslic3r/PrintConfig.hpp"

namespace Slic3r { namespace GUI {

// Presence marker in each range's ModelConfig. Value is always "".
// Lets us distinguish our ranges from Orca's height-painting ranges.
static constexpr const char* KEY_Z_PRESET_NAME = "z_preset_name";

// ── ZRegionPreview ────────────────────────────────────────────────────────────
class ZRegionPreview : public wxPanel
{
public:
    struct Band {
        double   z_min { 0.0 };
        double   z_max { 0.0 };
        wxString label;
        wxColour color;
        int      n_overrides { 0 };
    };

    ZRegionPreview(wxWindow* parent, double obj_height)
        : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(160, -1))
        , m_obj_height(obj_height)
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        Bind(wxEVT_PAINT,        &ZRegionPreview::on_paint,      this);
        Bind(wxEVT_SIZE,         [this](wxSizeEvent& e){ Refresh(); e.Skip(); });
        Bind(wxEVT_LEFT_DOWN,    &ZRegionPreview::on_mouse_down, this);
        Bind(wxEVT_LEFT_UP,      &ZRegionPreview::on_mouse_up,   this);
        Bind(wxEVT_MOTION,       &ZRegionPreview::on_mouse_move, this);
        Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent&){ SetCursor(wxNullCursor); });
    }

    void set_bands(const std::vector<Band>& bands) { m_bands = bands; Refresh(); }
    void set_selected_band(int idx) { m_selected_band = idx; Refresh(); }

    static wxColour band_color(size_t idx) {
        static const wxColour pal[] = {
            {0x4E,0x79,0xA7},{0xF2,0x8E,0x2B},{0x59,0xA1,0x4F},
            {0xE1,0x57,0x59},{0xB0,0x7A,0xA1},{0x76,0xB7,0xB2},{0xFF,0xD7,0x00},
        };
        return pal[idx % (sizeof(pal)/sizeof(pal[0]))];
    }

    std::function<void(int,double)> m_on_boundary_drag;
    int m_selected_band { -1 };
    void set_on_boundary_drag(std::function<void(int,double)> cb){ m_on_boundary_drag=cb; }

private:
    double            m_obj_height{1.0};
    std::vector<Band> m_bands;
    int               m_drag_boundary{-1};

    void on_mouse_down(wxMouseEvent& e){
        int b=boundary_at(e.GetY());
        if(b>=0){m_drag_boundary=b;CaptureMouse();SetCursor(wxCursor(wxCURSOR_SIZENS));}
        e.Skip();
    }
    void on_mouse_up(wxMouseEvent& e){
        if(m_drag_boundary>=0){if(HasCapture())ReleaseMouse();m_drag_boundary=-1;SetCursor(wxNullCursor);}
        e.Skip();
    }
    void on_mouse_move(wxMouseEvent& e){
        if(m_drag_boundary>=0&&e.LeftIsDown()){
            double cz=std::max(0.0,std::min(m_obj_height,y_to_z(e.GetY())));
            if(m_on_boundary_drag) m_on_boundary_drag(m_drag_boundary,cz);
            if(m_drag_boundary  <(int)m_bands.size()) m_bands[m_drag_boundary].z_max  =cz;
            if(m_drag_boundary+1<(int)m_bands.size()) m_bands[m_drag_boundary+1].z_min=cz;
            Refresh();
        } else {
            SetCursor(boundary_at(e.GetY())>=0?wxCursor(wxCURSOR_SIZENS):wxNullCursor);
        }
        e.Skip();
    }
    int boundary_at(int my) const {
        const wxSize sz=GetClientSize();const int padT=20,padB=20,barH=sz.y-padT-padB;
        if(barH<=0||m_obj_height<=0) return -1;
        for(int i=0;i+1<(int)m_bands.size();++i){
            int by=padT+barH-(int)(m_bands[i].z_max/m_obj_height*barH);
            if(std::abs(my-by)<=6) return i;
        }
        return -1;
    }
    double y_to_z(int my) const {
        const wxSize sz=GetClientSize();const int padT=20,padB=20,barH=sz.y-padT-padB;
        return barH<=0?0.0:m_obj_height*(1.0-double(my-padT)/barH);
    }
    void on_paint(wxPaintEvent&){
        wxAutoBufferedPaintDC dc(this);
        dc.SetBackground(wxBrush(GetParent()->GetBackgroundColour()));dc.Clear();
        const wxSize sz=GetClientSize();
        const int padT=20,padB=20,padL=8,barW=40,barH=sz.y-padT-padB,barX=padL;
        if(barH<=0||m_obj_height<=0) return;
        auto z2y=[&](double z)->int{return padT+barH-(int)(z/m_obj_height*barH);};
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(wxColour(0xCC,0xCC,0xCC)));
        dc.DrawRectangle(barX,padT,barW,barH);
        dc.SetFont(wxFont(7,wxFONTFAMILY_DEFAULT,wxFONTSTYLE_NORMAL,wxFONTWEIGHT_NORMAL));
        for(size_t i=0;i<m_bands.size();++i){
            const Band&b=m_bands[i];
            if(b.z_max<=b.z_min) continue;
            int yt=z2y(b.z_max),yb=z2y(b.z_min),h=std::max(2,yb-yt);
            bool sel=((int)i==m_selected_band);
            wxColour fill=b.n_overrides>0?b.color:wxColour(0xCC,0xCC,0xCC);
            dc.SetBrush(wxBrush(sel?fill.ChangeLightness(140):fill));
            dc.SetPen(wxPen(sel?*wxWHITE:fill.ChangeLightness(70),sel?2:1));
            dc.DrawRectangle(barX,yt,barW,h);
            if(b.z_min>0.001){
                bool drag=(boundary_at(yb)>=0);
                dc.SetPen(wxPen(drag?wxColour(0x25,0x88,0xFF):wxColour(80,80,80),drag?2:1));
                dc.DrawLine(barX-3,yb,barX+barW+6,yb);
                if(drag||m_drag_boundary==(int)i-1){
                    dc.SetBrush(wxBrush(wxColour(0x25,0x88,0xFF)));
                    dc.SetPen(*wxTRANSPARENT_PEN);
                    wxPoint tri[3]={{barX-3,yb-5},{barX-3,yb+5},{barX+barW+6,yb}};
                    dc.DrawPolygon(3,tri);
                }
                dc.SetTextForeground(wxColour(60,60,60));
                dc.DrawText(wxString::Format("%.1f",b.z_min),barX+barW+8,yb-7);
            }
            if(h>12){
                dc.SetTextForeground(b.n_overrides>0?*wxWHITE:wxColour(80,80,80));
                wxString lbl=b.n_overrides>0?wxString::Format("%d",b.n_overrides):wxString("-");
                wxCoord tw,th;dc.GetTextExtent(lbl,&tw,&th);
                dc.DrawText(lbl,barX+(barW-tw)/2,yt+(h-th)/2);
            }
        }
        dc.SetPen(wxPen(wxColour(80,80,80),1));
        dc.DrawLine(barX,padT,barX+barW+4,padT);
        dc.DrawLine(barX,padT+barH,barX+barW+4,padT+barH);
        dc.SetTextForeground(wxColour(60,60,60));
        dc.DrawText(wxString::Format("%.1f",m_obj_height),barX+barW+6,padT-8);
        dc.DrawText("0.0",barX+barW+6,padT+barH-2);
        dc.SetFont(wxFont(7,wxFONTFAMILY_DEFAULT,wxFONTSTYLE_ITALIC,wxFONTWEIGHT_NORMAL));
        dc.SetTextForeground(wxColour(100,100,100));
        dc.DrawRotatedText("Z (mm from base)",10,padT+barH/2+30,90);
    }
};

// ── ZPresetRegionsDialog ──────────────────────────────────────────────────────
class ZPresetRegionsDialog : public wxDialog
{
public:
    explicit ZPresetRegionsDialog(wxWindow* parent, int obj_idx);

private:
    struct Row {
        double      z_min { 0.0 };
        double      z_max { 0.0 };
        std::string label;
        std::map<std::string,std::string> overrides; // key → serialized value
    };
    struct KeyCtrl {
        std::string key;
        wxCheckBox* cb       { nullptr };
        wxWindow*   val_ctrl { nullptr };
    };

    int              m_obj_idx      { -1 };
    int              m_selected_row { -1 };
    std::vector<Row>     m_rows;
    std::vector<KeyCtrl> m_key_ctrls;

    wxGrid*           m_grid      { nullptr };
    wxScrolledWindow* m_ovr_panel { nullptr };
    ZRegionPreview*   m_preview   { nullptr };
    wxButton*         m_btn_add   { nullptr };
    wxButton*         m_btn_del   { nullptr };
    wxButton*         m_btn_ok    { nullptr };
    wxStaticText*     m_ovr_hint  { nullptr };

    void build_ui();
    void build_override_panel(wxWindow* parent, wxSizer* into);
    void populate_from_model();
    void apply_to_model();
    void refresh_grid();
    void refresh_preview();
    void highlight_band_in_canvas(int row);
    void load_overrides_for_row(int row);
    void save_overrides_from_panel(int row);
    void set_override_panel_enabled(bool en);
    void on_add(wxCommandEvent&);
    void on_del(wxCommandEvent&);
    void on_ok (wxCommandEvent&);
};

void open_z_preset_regions_dialog(int obj_idx);

}} // namespace Slic3r::GUI
