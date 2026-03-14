// ─── ORCA FullSpectrum: Z-Override Regions — v2 (key overrides) ──────────────
#include "ZPresetRegions.hpp"
#include "GUI_App.hpp"
#include "Plater.hpp"
#include "libslic3r/PresetBundle.hpp"
#include "libslic3r/PrintConfig.hpp"

#include <wx/statline.h>
#include <wx/scrolwin.h>
#include <fstream>
#include <algorithm>

namespace Slic3r { namespace GUI {

// ── Key group definitions ─────────────────────────────────────────────────────
struct KeyDef   { const char* key; const char* label; };
struct GroupDef { const char* name; std::vector<KeyDef> keys; };

static const GroupDef s_groups[] = {
  { "Walls", {
    {"wall_loops",                "Wall loops"},
    {"wall_sequence",             "Wall order"},
    {"wall_direction",            "Wall direction"},
    {"outer_wall_line_width",     "Outer wall line width"},
    {"inner_wall_line_width",     "Inner wall line width"},
    {"outer_wall_speed",          "Outer wall speed"},
    {"inner_wall_speed",          "Inner wall speed"},
    {"outer_wall_flow_ratio",     "Outer wall flow ratio"},
    {"wall_filament",             "Wall filament"},
    {"only_one_wall_top",         "One wall on top"},
    {"only_one_wall_first_layer", "One wall first layer"},
    {"detect_thin_wall",          "Detect thin wall"},
    {"detect_overhang_wall",      "Detect overhang wall"},
    {"overhang_reverse",          "Reverse overhang walls"},
  }},
  { "Seam", {
    {"seam_position",             "Seam position"},
    {"seam_gap",                  "Seam gap"},
    {"wipe_on_loops",             "Wipe on loops"},
    {"wipe_before_external_loop", "Wipe before external loop"},
    {"wipe_speed",                "Wipe speed"},
  }},
  { "Infill", {
    {"sparse_infill_density",        "Infill density"},
    {"sparse_infill_pattern",        "Infill pattern"},
    {"sparse_infill_line_width",     "Infill line width"},
    {"sparse_infill_speed",          "Infill speed"},
    {"sparse_infill_filament",       "Infill filament"},
    {"sparse_infill_rotate_template","Rotate infill template"},
    {"infill_combination",           "Infill combination"},
    {"infill_anchor",                "Infill anchor length"},
    {"infill_anchor_max",            "Infill anchor max"},
    {"infill_wall_overlap",          "Infill/wall overlap"},
    {"minimum_sparse_infill_area",   "Min sparse infill area"},
    {"symmetric_infill_y_axis",      "Symmetric infill Y"},
    {"solid_infill_direction",       "Solid infill direction"},
    {"solid_infill_filament",        "Solid infill filament"},
    {"solid_infill_rotate_template", "Rotate solid template"},
    {"internal_solid_infill_speed",  "Internal solid infill speed"},
  }},
  { "Top / Bottom", {
    {"top_shell_layers",              "Top shell layers"},
    {"top_shell_thickness",           "Top shell thickness"},
    {"bottom_shell_layers",           "Bottom shell layers"},
    {"bottom_shell_thickness",        "Bottom shell thickness"},
    {"top_surface_pattern",           "Top surface pattern"},
    {"bottom_surface_pattern",        "Bottom surface pattern"},
    {"top_surface_line_width",        "Top surface line width"},
    {"top_surface_speed",             "Top surface speed"},
    {"top_surface_density",           "Top surface density"},
    {"top_solid_infill_flow_ratio",   "Top infill flow ratio"},
    {"top_bottom_infill_wall_overlap","Top/bottom wall overlap"},
  }},
  { "Ironing", {
    {"ironing_type",    "Ironing type"},
    {"ironing_speed",   "Ironing speed"},
    {"ironing_flow",    "Ironing flow"},
    {"ironing_spacing", "Ironing spacing"},
    {"ironing_inset",   "Ironing inset"},
  }},
  { "Speed & Bridges", {
    {"bridge_speed",            "Bridge speed"},
    {"gap_infill_speed",        "Gap fill speed"},
    {"small_perimeter_speed",   "Small perimeter speed"},
    {"small_perimeter_threshold","Small perimeter threshold"},
    {"enable_overhang_speed",   "Enable overhang speed"},
    {"overhangs_speed_classic", "Overhang speed (classic)"},
  }},
  { "Flow", {
    {"print_flow_ratio", "Print flow ratio"},
    {"bridge_flow",      "Bridge flow ratio"},
  }},
  { "Fuzzy Skin", {
    {"fuzzy_skin",                "Fuzzy skin type"},
    {"fuzzy_skin_thickness",      "Fuzzy skin thickness"},
    {"fuzzy_skin_point_distance", "Fuzzy skin point distance"},
  }},
  { "Layer Height", {
    {"layer_height", "Layer height"},
  }},
  { "Advanced", {
    {"filter_out_gap_fill",  "Filter gap fill (mm)"},
    {"gap_closing_radius",   "Gap closing radius"},
    {"enable_arc_fitting",   "Arc fitting"},
  }},
  { "Cooling & Temp", {
    {"nozzle_temperature",      "Nozzle temperature"},
    {"fan_always_on",           "Fan always on"},
    {"min_fan_speed",           "Min fan speed"},
  }},
};

// Helper: get the serialized default value for a key from the current print preset
static std::string default_val_for_key(const std::string& key)
{
    const auto& preset_cfg =
        wxGetApp().preset_bundle->prints.get_edited_preset().config;
    const ConfigOption* opt = preset_cfg.option(key);
    if (opt) return opt->serialize();

    const auto& fil_cfg =
        wxGetApp().preset_bundle->filaments.get_edited_preset().config;
    opt = fil_cfg.option(key);
    if (opt) return opt->serialize();
    // Fallback: PrintRegionConfig default
    static const PrintRegionConfig s_default;
    const ConfigOption* dopt = s_default.optptr(key);
    return dopt ? dopt->serialize() : "";
}

// ── Enum choices for Z-override dropdowns ─────────────────────────────────────
// Maps serialized key name → list of serialized enum values (what serialize() returns).
// Only keys whose values cannot be typed as free text need entries here.
static const std::map<std::string, std::vector<std::string>>& enum_choices()
{
    static const std::map<std::string, std::vector<std::string>> s_map = {
        { "seam_position",          { "nearest", "aligned", "aligned_back", "back", "random" } },
        { "wall_sequence",          { "inner wall/outer wall", "outer wall/inner wall", "inner-outer-inner wall" } },
        { "wall_direction",         { "auto", "ccw", "cw" } },
        { "ironing_type",           { "no ironing", "top", "topmost", "solid" } },
        { "fuzzy_skin",             { "none", "external", "all", "allwalls" } },
        { "sparse_infill_pattern",  {
            "monotonic","monotonicline","rectilinear","alignedrectilinear","zigzag",
            "crosszag","lockedzag","line","grid","triangles","tri-hexagon","cubic",
            "adaptivecubic","quartercubic","supportcubic","lightning","honeycomb",
            "3dhoneycomb","lateral-honeycomb","lateral-lattice","crosshatch",
            "tpmsd","tpmsfk","gyroid","concentric","hilbertcurve",
            "archimedeanchords","octagramspiral" } },
        { "top_surface_pattern",    {
            "monotonic","monotonicline","rectilinear","alignedrectilinear","zigzag",
            "crosszag","concentric","hilbertcurve","archimedeanchords","octagramspiral" } },
        { "bottom_surface_pattern", {
            "monotonic","monotonicline","rectilinear","alignedrectilinear","zigzag",
            "crosszag","concentric","hilbertcurve","archimedeanchords","octagramspiral" } },
    };
    return s_map;
}

// Helper: return config option type for a PrintRegionConfig key
static ConfigOptionType key_type(const std::string& key)
{
    static const PrintRegionConfig s_cfg;
    const ConfigOption* opt = s_cfg.optptr(key);
    return opt ? opt->type() : coNone;
}

// ── Constructor ───────────────────────────────────────────────────────────────

ZPresetRegionsDialog::ZPresetRegionsDialog(wxWindow* parent, int obj_idx)
    : wxDialog(parent, wxID_ANY, _L("Z-Override Regions"),
               wxDefaultPosition, wxSize(1020, 560),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    , m_obj_idx(obj_idx)
{
    populate_from_model();
    build_ui();
}

// ── populate_from_model ───────────────────────────────────────────────────────

void ZPresetRegionsDialog::populate_from_model()
{
    m_rows.clear();
    ModelObject* obj = wxGetApp().model().objects[m_obj_idx];

    // Read existing z-override ranges (those with our marker key)
    for (const auto& [range, cfg] : obj->layer_config_ranges) {
        if (!cfg.has(KEY_Z_PRESET_NAME)) continue;
        Row row;
        row.z_min = range.first;
        row.z_max = range.second;
        // Rebuild label and overrides from stored keys
        // (marker key itself is skipped)
        for (const auto& key : cfg.keys()) {
            if (key == KEY_Z_PRESET_NAME) continue;
            const ConfigOption* opt = cfg.option(key);
            if (opt) row.overrides[key] = opt->serialize();
        }
        // Label was stored specially in the z_preset_name key.
        std::string stored_label = cfg.get().opt_string(KEY_Z_PRESET_NAME);
        row.label = stored_label.empty() ? "Zone " + std::to_string(m_rows.size() + 1) : stored_label;
        m_rows.push_back(std::move(row));
    }

    const double obj_h = obj->raw_bounding_box().size().z();

    if (m_rows.empty()) {
        // Default: one zone covering the full object (= no split yet)
        m_rows.push_back({ 0.0, obj_h > 0 ? obj_h : 10.0, "Zone 1", {} });
    } else {
        // Ensure zones are sorted and span [0, obj_height]
        std::sort(m_rows.begin(), m_rows.end(),
            [](const Row& a, const Row& b){ return a.z_min < b.z_min; });
        m_rows.front().z_min = 0.0;
        m_rows.back().z_max  = obj_h > 0 ? obj_h : m_rows.back().z_max;
    }
}

// ── build_ui ──────────────────────────────────────────────────────────────────

void ZPresetRegionsDialog::build_ui()
{
    auto* root = new wxBoxSizer(wxVERTICAL);

    // Header
    auto* hdr = new wxStaticText(this, wxID_ANY,
        _L("Define height zones and select which print parameters to override in each zone.\n"
           "Z values are measured from the object base (0 = bottom). "
           "Check any parameter to activate it for the selected zone."));
    root->Add(hdr, 0, wxALL, 10);
    root->Add(new wxStaticLine(this), 0, wxEXPAND | wxLEFT | wxRIGHT, 10);

    // ── 3-column body ─────────────────────────────────────────────────────────
    auto* body = new wxBoxSizer(wxHORIZONTAL);

    // ── LEFT: zone grid + add/del ─────────────────────────────────────────────
    auto* left = new wxBoxSizer(wxVERTICAL);

    m_grid = new wxGrid(this, wxID_ANY, wxDefaultPosition, wxSize(290, -1));
    m_grid->CreateGrid(0, 3);
    m_grid->SetColLabelValue(0, _L("Z min (mm)"));
    m_grid->SetColLabelValue(1, _L("Z max (mm)"));
    m_grid->SetColLabelValue(2, _L("Label"));
    m_grid->SetColSize(0, 72); m_grid->SetColSize(1, 72); m_grid->SetColSize(2, 110);
    m_grid->SetRowLabelSize(28);
    m_grid->EnableEditing(true);
    m_grid->DisableDragColSize();
    m_grid->DisableDragRowSize();
    left->Add(m_grid, 1, wxEXPAND);

    auto* zone_btns = new wxBoxSizer(wxHORIZONTAL);
    m_btn_add = new wxButton(this, wxID_ANY, _L("+ Add Zone"),    wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
    m_btn_del = new wxButton(this, wxID_ANY, _L("- Remove Zone"), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
    zone_btns->Add(m_btn_add, 0, wxRIGHT, 4);
    zone_btns->Add(m_btn_del, 0);
    left->Add(zone_btns, 0, wxTOP, 4);

    body->Add(left, 0, wxEXPAND | wxRIGHT, 8);

    // ── CENTRE: override panel ────────────────────────────────────────────────
    auto* centre = new wxBoxSizer(wxVERTICAL);

    m_ovr_hint = new wxStaticText(this, wxID_ANY,
        _L("← Select a zone to configure its overrides"));
    m_ovr_hint->SetForegroundColour(wxColour(120,120,120));
    centre->Add(m_ovr_hint, 0, wxBOTTOM, 4);

    m_ovr_panel = new wxScrolledWindow(this, wxID_ANY,
        wxDefaultPosition, wxSize(500, -1),
        wxVSCROLL | wxBORDER_SIMPLE);
    m_ovr_panel->SetScrollRate(0, 12);
    build_override_panel(m_ovr_panel, centre);

    body->Add(centre, 1, wxEXPAND | wxRIGHT, 8);

    // ── RIGHT: preview ────────────────────────────────────────────────────────
    const double obj_h = wxGetApp().model().objects[m_obj_idx]->raw_bounding_box().size().z();
    m_preview = new ZRegionPreview(this, obj_h > 0 ? obj_h : 10.0);
    m_preview->set_on_boundary_drag([this](int bi, double nz){
        if (bi < 0 || bi + 1 >= (int)m_rows.size()) return;
        m_rows[bi].z_max       = nz;
        m_rows[bi+1].z_min     = nz;
        m_grid->SetCellValue(bi,   1, wxString::Format("%.3f", nz));
        m_grid->SetCellValue(bi+1, 0, wxString::Format("%.3f", nz));
        refresh_preview();
        highlight_band_in_canvas(m_selected_row);
    });
    body->Add(m_preview, 0, wxEXPAND);

    root->Add(body, 1, wxEXPAND | wxLEFT | wxRIGHT, 10);

    // ── Bottom buttons ────────────────────────────────────────────────────────
    auto* bot = new wxBoxSizer(wxHORIZONTAL);
    bot->AddStretchSpacer();
    auto* btn_cancel = new wxButton(this, wxID_CANCEL, _L("Cancel"));
    m_btn_ok         = new wxButton(this, wxID_OK,     _L("Apply"));
    bot->Add(btn_cancel, 0, wxRIGHT, 6);
    bot->Add(m_btn_ok,   0);
    root->Add(bot, 0, wxEXPAND | wxALL, 10);

    SetSizer(root);
    refresh_grid();
    set_override_panel_enabled(false);

    // ── Event bindings ────────────────────────────────────────────────────────
    Bind(wxEVT_CLOSE_WINDOW, [](wxCloseEvent& e){
        auto* c = wxGetApp().plater()->get_view3D_canvas3D();
        if (c) c->clear_z_band_highlight();
        e.Skip();
    });
    Bind(wxEVT_BUTTON, [](wxCommandEvent& e){
        if (e.GetId() == wxID_CANCEL) {
            auto* c = wxGetApp().plater()->get_view3D_canvas3D();
            if (c) c->clear_z_band_highlight();
        }
        e.Skip();
    });

    m_btn_add->Bind(wxEVT_BUTTON, &ZPresetRegionsDialog::on_add, this);
    m_btn_del->Bind(wxEVT_BUTTON, &ZPresetRegionsDialog::on_del, this);
    m_btn_ok ->Bind(wxEVT_BUTTON, &ZPresetRegionsDialog::on_ok,  this);

    m_grid->Bind(wxEVT_GRID_CELL_CHANGED, [this](wxGridEvent& e){
        int row=e.GetRow(), col=e.GetCol();
        if (row<0||row>=(int)m_rows.size()){e.Skip();return;}
        if (col==0) {
            m_rows[row].z_min = wxAtof(m_grid->GetCellValue(row,0));
            if (row>0){ m_rows[row-1].z_max=m_rows[row].z_min;
                m_grid->SetCellValue(row-1,1,wxString::Format("%.3f",m_rows[row-1].z_max)); }
        } else if (col==1) {
            m_rows[row].z_max = wxAtof(m_grid->GetCellValue(row,1));
            if (row+1<(int)m_rows.size()){ m_rows[row+1].z_min=m_rows[row].z_max;
                m_grid->SetCellValue(row+1,0,wxString::Format("%.3f",m_rows[row+1].z_min)); }
        } else if (col==2) {
            m_rows[row].label = m_grid->GetCellValue(row,2).ToUTF8().data();
        }
        refresh_preview();
        e.Skip();
    });

    m_grid->Bind(wxEVT_GRID_SELECT_CELL, [this](wxGridEvent& e){
        int prev = m_selected_row;
        int next = e.GetRow();
        if (prev>=0 && prev<(int)m_rows.size())
            save_overrides_from_panel(prev);
        m_selected_row = next;
        if (m_preview) m_preview->set_selected_band(next);
        highlight_band_in_canvas(next);
        load_overrides_for_row(next);
        e.Skip();
    });
}

// ── build_override_panel ──────────────────────────────────────────────────────

void ZPresetRegionsDialog::build_override_panel(wxWindow* parent, wxSizer* into)
{
    auto* panel_sizer = new wxBoxSizer(wxVERTICAL);

    // We'll use a 3-col FlexGridSizer per group:
    // col0: checkbox (20px), col1: label (expand), col2: value ctrl (120px)
    const PrintRegionConfig s_region_cfg; // for type detection (unused directly, just ensure compile)

    for (const GroupDef& grp : s_groups) {
        // Group header
        auto* hdr = new wxStaticText(parent, wxID_ANY, wxString::FromUTF8(grp.name));
        wxFont f = hdr->GetFont();
        f.SetWeight(wxFONTWEIGHT_BOLD);
        hdr->SetFont(f);
        hdr->SetForegroundColour(wxColour(0x25, 0x88, 0xFF));
        panel_sizer->Add(hdr, 0, wxTOP | wxBOTTOM | wxLEFT, 6);

        auto* grid = new wxFlexGridSizer(3, 4, 2);
        grid->AddGrowableCol(1, 1);

        for (const KeyDef& kd : grp.keys) {
            // Skip keys not present in PrintRegionConfig
            // (we check at runtime so unknown keys fail gracefully)
            const std::string key(kd.key);

            auto* cb  = new wxCheckBox(parent, wxID_ANY, wxEmptyString);
            auto* lbl = new wxStaticText(parent, wxID_ANY, wxString::FromUTF8(kd.label));

            wxWindow* val_ctrl = nullptr;
            const ConfigOptionType ktype = key_type(key);
            const auto& ecmap = enum_choices();
            auto ecit = ecmap.find(key);
            const bool is_enum = (ecit != ecmap.end());
            const bool is_bool = (ktype == coBool);

            if (is_enum || is_bool) {
                auto* ch = new wxChoice(parent, wxID_ANY);
                ch->SetMinSize(wxSize(190, -1));
                if (is_bool) {
                    ch->Append("false");
                    ch->Append("true");
                    ch->SetSelection(0);
                } else {
                    for (const auto& s : ecit->second)
                        ch->Append(wxString::FromUTF8(s));
                    ch->SetSelection(0);
                }
                val_ctrl = ch;
            } else {
                auto* tc = new wxTextCtrl(parent, wxID_ANY, wxEmptyString,
                    wxDefaultPosition, wxSize(190, -1));
                val_ctrl = tc;
            }
            val_ctrl->Enable(false);

            grid->Add(cb,       0, wxALIGN_CENTER_VERTICAL);
            grid->Add(lbl,      1, wxALIGN_CENTER_VERTICAL | wxLEFT, 2);
            grid->Add(val_ctrl, 0, wxALIGN_CENTER_VERTICAL);

            // Wire: toggling checkbox enables value ctrl and pre-fills with global default
            cb->Bind(wxEVT_CHECKBOX, [this, cb, val_ctrl, key](wxCommandEvent&){
                bool chk = cb->IsChecked();
                val_ctrl->Enable(chk);
                if (chk) {
                    std::string def = default_val_for_key(key);
                    if (auto* tc = dynamic_cast<wxTextCtrl*>(val_ctrl)) {
                        tc->SetValue(wxString::FromUTF8(def));
                    } else if (auto* ch = dynamic_cast<wxChoice*>(val_ctrl)) {
                        // Match serialized default to choice list
                        bool matched = false;
                        for (unsigned i = 0; i < ch->GetCount(); ++i) {
                            if (ch->GetString(i).ToUTF8().data() == def ||
                                (def=="1"&&ch->GetString(i)=="true") ||
                                (def=="0"&&ch->GetString(i)=="false")) {
                                ch->SetSelection((int)i);
                                matched = true;
                                break;
                            }
                        }
                        if (!matched) ch->SetSelection(0);
                    }
                }
                if (m_selected_row >= 0 && m_selected_row < (int)m_rows.size())
                    save_overrides_from_panel(m_selected_row);
                refresh_preview();
            });

            m_key_ctrls.push_back({ key, cb, val_ctrl });
        }

        panel_sizer->Add(grid, 0, wxEXPAND | wxLEFT | wxRIGHT, 8);
        panel_sizer->Add(new wxStaticLine(parent), 0, wxEXPAND | wxTOP | wxBOTTOM | wxLEFT | wxRIGHT, 4);
    }

    parent->SetSizer(panel_sizer);
    parent->FitInside();
    into->Add(parent, 1, wxEXPAND);
}

// ── load_overrides_for_row ────────────────────────────────────────────────────

void ZPresetRegionsDialog::load_overrides_for_row(int row)
{
    const bool valid = row >= 0 && row < (int)m_rows.size();
    set_override_panel_enabled(valid);
    if (m_ovr_hint) m_ovr_hint->Show(!valid);

    if (!valid) {
        for (auto& kc : m_key_ctrls) {
            kc.cb->SetValue(false);
            kc.val_ctrl->Enable(false);
        }
        return;
    }

    const auto& ovr = m_rows[row].overrides;
    for (auto& kc : m_key_ctrls) {
        auto it = ovr.find(kc.key);
        bool has = (it != ovr.end());
        kc.cb->SetValue(has);
        kc.val_ctrl->Enable(has);

        // Value: show saved value if exists, otherwise show global default
        std::string val = has ? it->second : default_val_for_key(kc.key);
        if (auto* tc = dynamic_cast<wxTextCtrl*>(kc.val_ctrl)) {
            tc->SetValue(wxString::FromUTF8(val));
        } else if (auto* ch = dynamic_cast<wxChoice*>(kc.val_ctrl)) {
            bool matched = false;
            for (unsigned i = 0; i < ch->GetCount(); ++i) {
                if (ch->GetString(i).ToUTF8().data() == val ||
                    (val=="1"&&ch->GetString(i)=="true") ||
                    (val=="0"&&ch->GetString(i)=="false")) {
                    ch->SetSelection((int)i);
                    matched = true;
                    break;
                }
            }
            if (!matched) ch->SetSelection(0);
        }
    }
}

// ── save_overrides_from_panel ─────────────────────────────────────────────────

void ZPresetRegionsDialog::save_overrides_from_panel(int row)
{
    if (row < 0 || row >= (int)m_rows.size()) return;
    m_rows[row].overrides.clear();
    for (const auto& kc : m_key_ctrls) {
        if (!kc.cb->IsChecked()) continue;
        std::string val;
        if (auto* tc = dynamic_cast<wxTextCtrl*>(kc.val_ctrl)) {
            val = tc->GetValue().ToUTF8().data();
        } else if (auto* ch = dynamic_cast<wxChoice*>(kc.val_ctrl)) {
            // Bool choices store "true"/"false"; enum choices store the serialized string
            wxString sel = ch->GetStringSelection();
            val = sel.ToUTF8().data();
            // Normalize bools
            if (val == "true")  val = "true";
            if (val == "false") val = "false";
        }
        if (!val.empty())
            m_rows[row].overrides[kc.key] = val;
    }
}

// ── set_override_panel_enabled ────────────────────────────────────────────────

void ZPresetRegionsDialog::set_override_panel_enabled(bool en)
{
    if (m_ovr_panel) m_ovr_panel->Enable(en);
}

// ── highlight_band_in_canvas ──────────────────────────────────────────────────

void ZPresetRegionsDialog::highlight_band_in_canvas(int row)
{
    auto* canvas = wxGetApp().plater()->get_view3D_canvas3D();
    if (!canvas) return;
    if (row < 0 || row >= (int)m_rows.size()) { canvas->clear_z_band_highlight(); return; }
    const Row& r = m_rows[row];
    ModelObject* obj = wxGetApp().model().objects[m_obj_idx];
    if (!obj || obj->instances.empty()) return;
    const BoundingBoxf3 wb = obj->instance_bounding_box(0);
    canvas->set_z_band_highlight(
        (float)(wb.min.z() + r.z_min), (float)(wb.min.z() + r.z_max),
        (float)wb.min.x(), (float)wb.min.y(),
        (float)wb.max.x(), (float)wb.max.y());
}

// ── refresh_preview ───────────────────────────────────────────────────────────

void ZPresetRegionsDialog::refresh_preview()
{
    if (!m_preview) return;
    std::vector<ZRegionPreview::Band> bands;
    for (size_t i = 0; i < m_rows.size(); ++i) {
        const Row& r = m_rows[i];
        ZRegionPreview::Band b;
        b.z_min       = r.z_min;
        b.z_max       = r.z_max;
        b.n_overrides = (int)r.overrides.size();
        b.color       = ZRegionPreview::band_color(i);
        b.label       = r.label.empty()
            ? wxString::Format("Zone %d", (int)i+1)
            : wxString::FromUTF8(r.label);
        bands.push_back(b);
    }
    m_preview->set_bands(bands);
}

// ── refresh_grid ──────────────────────────────────────────────────────────────

void ZPresetRegionsDialog::refresh_grid()
{
    if (m_grid->GetNumberRows() > 0)
        m_grid->DeleteRows(0, m_grid->GetNumberRows());

    std::sort(m_rows.begin(), m_rows.end(), [](const Row& a, const Row& b){
        return a.z_min < b.z_min;
    });

    for (size_t i = 0; i < m_rows.size(); ++i) {
        m_grid->AppendRows(1);
        const Row& r = m_rows[i];
        m_grid->SetCellValue((int)i, 0, wxString::Format("%.3f", r.z_min));
        m_grid->SetCellValue((int)i, 1, wxString::Format("%.3f", r.z_max));
        wxString lbl = r.label.empty()
            ? wxString::Format("Zone %d", (int)i+1)
            : wxString::FromUTF8(r.label);
        // Append override count hint
        if (!r.overrides.empty())
            lbl += wxString::Format(" (%d)", (int)r.overrides.size());
        m_grid->SetCellValue((int)i, 2, lbl);
        m_grid->SetCellEditor((int)i, 0, new wxGridCellFloatEditor(-1, 3));
        m_grid->SetCellEditor((int)i, 1, new wxGridCellFloatEditor(-1, 3));
    }
    refresh_preview();
}

// ── on_add ────────────────────────────────────────────────────────────────────

void ZPresetRegionsDialog::on_add(wxCommandEvent&)
{
    if (m_rows.empty()) return;

    // Split the selected zone (or last zone) exactly in half
    int target = (m_selected_row >= 0 && m_selected_row < (int)m_rows.size())
        ? m_selected_row : (int)m_rows.size() - 1;

    // Save current panel state before modifying rows
    if (m_selected_row >= 0 && m_selected_row < (int)m_rows.size())
        save_overrides_from_panel(m_selected_row);

    Row& src = m_rows[target];
    double mid = src.z_min + (src.z_max - src.z_min) * 0.5;
    if (src.z_max - src.z_min < 0.1) {
        wxMessageBox(_L("Zone is too thin to split (< 0.1 mm)."),
                     _L("Z-Override Regions"), wxOK | wxICON_WARNING);
        return;
    }

    Row new_row;
    new_row.z_min = mid;
    new_row.z_max = src.z_max;
    new_row.label = "Zone " + std::to_string(m_rows.size() + 1);
    // Inherit overrides from parent zone as a starting point
    new_row.overrides = src.overrides;

    src.z_max = mid; // shrink original zone

    m_rows.insert(m_rows.begin() + target + 1, std::move(new_row));

    m_selected_row = target + 1;
    refresh_grid();
    load_overrides_for_row(m_selected_row);
    if (m_preview) m_preview->set_selected_band(m_selected_row);
    highlight_band_in_canvas(m_selected_row);
}

// ── on_del ────────────────────────────────────────────────────────────────────

void ZPresetRegionsDialog::on_del(wxCommandEvent&)
{
    int row = m_grid->GetGridCursorRow();
    if (row < 0 || row >= (int)m_rows.size()) return;

    if (m_rows.size() == 1) {
        // Last zone — ask user whether to disable Z-overrides entirely
        int answer = wxMessageBox(
            _L("This is the last zone. Removing it will disable all Z-overrides for this object.\n\nProceed?"),
            _L("Remove All Z-Overrides"), wxYES_NO | wxICON_QUESTION);
        if (answer != wxYES) return;
        // Clear everything and close
        ModelObject* obj = wxGetApp().model().objects[m_obj_idx];
        if (obj) {
            t_layer_config_ranges cleaned;
            for (const auto& [range, cfg] : obj->layer_config_ranges)
                if (!cfg.has(KEY_Z_PRESET_NAME))
                    cleaned[range] = cfg;
            obj->layer_config_ranges = std::move(cleaned);
        }
        wxGetApp().plater()->changed_object(m_obj_idx);
        wxGetApp().obj_list()->update_info_items(m_obj_idx);
        auto* canvas = wxGetApp().plater()->get_view3D_canvas3D();
        if (canvas) canvas->clear_z_band_highlight();
        EndModal(wxID_CANCEL); // close dialog — no further Apply needed
        return;
    }

    // Remove zone and expand the adjacent zone to fill the gap
    double z_min = m_rows[row].z_min;
    double z_max = m_rows[row].z_max;
    if (m_selected_row == row) {
        m_selected_row = -1;
        load_overrides_for_row(-1);
    }
    m_rows.erase(m_rows.begin() + row);

    // Expand neighbour to cover the removed zone's range
    if (row < (int)m_rows.size()) {
        // Deleted zone was above neighbour → expand neighbour upward
        m_rows[row].z_min = z_min;
    } else if (row > 0) {
        // Deleted zone was at bottom → expand zone below downward
        m_rows[row - 1].z_max = z_max;
    }
    refresh_grid();
}

// ── on_ok ─────────────────────────────────────────────────────────────────────

void ZPresetRegionsDialog::on_ok(wxCommandEvent&)
{
    if (m_grid->IsCellEditControlEnabled()) {
        m_grid->SaveEditControlValue();
        m_grid->DisableCellEditControl();
    }

    // Commit current panel state
    if (m_selected_row >= 0 && m_selected_row < (int)m_rows.size())
        save_overrides_from_panel(m_selected_row);

    // Read back Z values from grid
    for (int i = 0; i < m_grid->GetNumberRows() && i < (int)m_rows.size(); ++i) {
        m_rows[i].z_min = wxAtof(m_grid->GetCellValue(i, 0));
        m_rows[i].z_max = wxAtof(m_grid->GetCellValue(i, 1));
    }

    // Validate
    for (int i = 0; i < (int)m_rows.size(); ++i) {
        if (m_rows[i].z_max <= m_rows[i].z_min) {
            wxMessageBox(wxString::Format(
                _L("Zone %d: Z max (%.3f) must be greater than Z min (%.3f)."),
                i+1, m_rows[i].z_max, m_rows[i].z_min),
                _L("Z-Override Regions"), wxOK | wxICON_WARNING);
            return;
        }
    }

    refresh_preview();
    apply_to_model();

    auto* canvas = wxGetApp().plater()->get_view3D_canvas3D();
    if (canvas) canvas->clear_z_band_highlight();

    EndModal(wxID_OK);
}

// ── apply_to_model ────────────────────────────────────────────────────────────

void ZPresetRegionsDialog::apply_to_model()
{
    const auto& objs = wxGetApp().model().objects;
    if (m_obj_idx < 0 || m_obj_idx >= (int)objs.size()) return;
    ModelObject* obj = objs[m_obj_idx];
    if (!obj) return;

    // Keep non-z-override ranges (e.g. height painting)
    t_layer_config_ranges new_ranges;
    for (const auto& [range, cfg] : obj->layer_config_ranges)
        if (!cfg.has(KEY_Z_PRESET_NAME))
            new_ranges[range] = cfg;

    // Write our zones
    for (const Row& row : m_rows) {
        if (row.z_max <= row.z_min) continue;
        t_layer_height_range range { row.z_min, row.z_max };
        ModelConfig& cfg = new_ranges[range];
        cfg.reset();

        // Marker: we store the zone label in the value to persist it into the 3MF.
        cfg.set(KEY_Z_PRESET_NAME, row.label);

        if (!row.overrides.empty()) {
            // Build a DynamicPrintConfig from serialized strings and apply
            DynamicPrintConfig tmp;
            for (const auto& [key, val] : row.overrides) {
                try {
                    ConfigSubstitutionContext ctx(ForwardCompatibilitySubstitutionRule::Disable);
                    tmp.set_deserialize(key, val, ctx);
                } catch (...) {
                    // Invalid value for this key — skip silently
                }
            }
            if (!tmp.empty())
                cfg.apply(tmp, /*ignore_nonexistent=*/true);
        }
    }

    obj->layer_config_ranges = std::move(new_ranges);
}

// ── Entry point ───────────────────────────────────────────────────────────────

void open_z_preset_regions_dialog(int obj_idx)
{
    if (obj_idx < 0 || obj_idx >= (int)wxGetApp().model().objects.size()) return;
    ZPresetRegionsDialog dlg(wxGetApp().plater(), obj_idx);
    if (dlg.ShowModal() == wxID_OK) {
        if (obj_idx < (int)wxGetApp().model().objects.size()) {
            wxGetApp().plater()->changed_object(obj_idx);
            wxGetApp().obj_list()->update_info_items(obj_idx);
        }
    }
}

}} // namespace Slic3r::GUI
