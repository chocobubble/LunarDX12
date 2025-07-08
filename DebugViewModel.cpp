#include "DebugViewModel.h"

#include "ConstantBuffers.h"
#include "LunarGUI.h"
#include "LunarConstants.h"
#include "Logger.h"

using namespace std;
using namespace Lunar::LunarConstants;

namespace Lunar
{

void DebugViewModel::Initialize(LunarGui* gui, BasicConstants& constants)
{
    if (!gui)
    {
        LOG_ERROR("DebugViewModel: Invalid gui pointer");
        return;
    }

    m_constants = &constants;  // Store reference as pointer
    
    // Main window toggle - main category
    gui->BindCheckbox("Show Debug Window", &m_showDebugWindow, nullptr, "main");
    
    // Debug window elements in requested order - debug category
    gui->BindCheckbox("Show Normals", &m_showNormals, [constants = m_constants](bool value) {
        if (value)
            constants->debugFlags |= DebugFlags::SHOW_NORMALS;
        else
            constants->debugFlags &= ~DebugFlags::SHOW_NORMALS;
    }, "debug");
    
    gui->BindCheckbox("Enable Normal Mapping", &m_normalMapEnabled, [constants = m_constants](bool value) {
        if (value)
            constants->debugFlags |= DebugFlags::NORMAL_MAP_ENABLED;
        else
            constants->debugFlags &= ~DebugFlags::NORMAL_MAP_ENABLED;
    }, "debug");
    
    gui->BindCheckbox("Show UVs", &m_showUVs, [constants = m_constants](bool value) {
        if (value)
            constants->debugFlags |= DebugFlags::SHOW_UVS;
        else
            constants->debugFlags &= ~DebugFlags::SHOW_UVS;
    }, "debug");
    
    gui->BindCheckbox("Enable PBR", &m_pbrEnabled, [constants = m_constants](bool value) {
        if (value)
            constants->debugFlags |= DebugFlags::PBR_ENABLED;
        else
            constants->debugFlags &= ~DebugFlags::PBR_ENABLED;
    }, "debug");
    
    gui->BindCheckbox("Enable Shadows", &m_shadowsEnabled, [constants = m_constants](bool value) {
        if (value)
            constants->debugFlags |= DebugFlags::SHADOWS_ENABLED;
        else
            constants->debugFlags &= ~DebugFlags::SHADOWS_ENABLED;
    }, "debug");
    
    gui->BindCheckbox("Wireframe", &m_wireframe, [constants = m_constants](bool value) {
        if (value)
            constants->debugFlags |= DebugFlags::WIREFRAME;
        else
            constants->debugFlags &= ~DebugFlags::WIREFRAME;
    }, "debug");
    
    gui->BindCheckbox("Enable AO", &m_aoEnabled, [constants = m_constants](bool value) {
        if (value)
            constants->debugFlags |= DebugFlags::AO_ENABLED;
        else
            constants->debugFlags &= ~DebugFlags::AO_ENABLED;
    }, "debug");
    
    gui->BindCheckbox("Enable IBL", &m_iblEnabled, [constants = m_constants](bool value) {
        if (value)
            constants->debugFlags |= DebugFlags::IBL_ENABLED;
        else
            constants->debugFlags &= ~DebugFlags::IBL_ENABLED;
    }, "debug");

    gui->BindCheckbox("Enable Height Map", &m_heightMapEnabled, [constants = m_constants](bool value) {
        if (value)
            constants->debugFlags |= DebugFlags::HEIGHT_MAP_ENABLED;
        else
            constants->debugFlags &= ~DebugFlags::HEIGHT_MAP_ENABLED;
    }, "debug");

    gui->BindCheckbox("Enable Metallic Map", &m_metallicMapEnabled, [constants = m_constants](bool value) {
        if (value)
            constants->debugFlags |= DebugFlags::METALLIC_MAP_ENABLED;
        else
            constants->debugFlags &= ~DebugFlags::METALLIC_MAP_ENABLED;
    }, "debug");

    gui->BindCheckbox("Enable Roughness Map", &m_roughnessMapEnabled, [constants = m_constants](bool value) {
        if (value)
            constants->debugFlags |= DebugFlags::ROUGHNESS_MAP_ENABLED;
        else
            constants->debugFlags &= ~DebugFlags::ROUGHNESS_MAP_ENABLED;
    }, "debug");

    gui->BindCheckbox("Enable Albedo Map", &m_albedoMapEnabled, [constants = m_constants](bool value) {
        if (value)
            constants->debugFlags |= DebugFlags::ALBEDO_MAP_ENABLED;
        else
            constants->debugFlags &= ~DebugFlags::ALBEDO_MAP_ENABLED;
    }, "debug");
    
    // Debug window with elements in order
    vector<string> debugElementIds = {
        "Show Normals",
        "Enable Normal Mapping",
        "Show UVs",
        "Enable PBR",
        "Enable Shadows", 
        "Wireframe",
        "Enable AO",
        "Enable IBL",
        "Enable Height Map",
        "Enable Metallic Map",
        "Enable Roughness Map",
        "Enable Albedo Map"
    };
    
    gui->BindWindow("Debug Window", "Debug Controls", &m_showDebugWindow, debugElementIds);
    
    SyncFromConstants();
    
    LOG_DEBUG("DebugViewModel initialized successfully");
}

void DebugViewModel::SyncFromConstants()
{
    if (!m_constants) return;
    
    // Sync from constants to UI variables in order
    m_showNormals = m_constants->debugFlags & DebugFlags::SHOW_NORMALS;
    m_normalMapEnabled = m_constants->debugFlags & DebugFlags::NORMAL_MAP_ENABLED;
    m_showUVs = m_constants->debugFlags & DebugFlags::SHOW_UVS;
    m_pbrEnabled = m_constants->debugFlags & DebugFlags::PBR_ENABLED;
    m_shadowsEnabled = m_constants->debugFlags & DebugFlags::SHADOWS_ENABLED;
    m_wireframe = m_constants->debugFlags & DebugFlags::WIREFRAME;
    m_aoEnabled = m_constants->debugFlags & DebugFlags::AO_ENABLED;
    m_iblEnabled = m_constants->debugFlags & DebugFlags::IBL_ENABLED;
    m_heightMapEnabled = m_constants->debugFlags & DebugFlags::HEIGHT_MAP_ENABLED;
    m_metallicMapEnabled = m_constants->debugFlags & DebugFlags::METALLIC_MAP_ENABLED;
    m_roughnessMapEnabled = m_constants->debugFlags & DebugFlags::ROUGHNESS_MAP_ENABLED;
    m_albedoMapEnabled = m_constants->debugFlags & DebugFlags::ALBEDO_MAP_ENABLED;

}

} // namespace Lunar
