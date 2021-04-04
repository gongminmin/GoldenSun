set(gltf_sample_models_repo "https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Models/0bc2ad7535aba4e16fb1afba1f85fd827bbbea6d/")
set(asset_dir "${CMAKE_CURRENT_SOURCE_DIR}/Assets/")

DownloadFile("${gltf_sample_models_repo}2.0/DamagedHelmet/glTF/DamagedHelmet.bin" "${asset_dir}DamagedHelmet/DamagedHelmet.bin" "D4E4C168E9AF9D22502E80F81C4587B41BD69900")
DownloadFile("${gltf_sample_models_repo}2.0/DamagedHelmet/glTF/DamagedHelmet.gltf" "${asset_dir}DamagedHelmet/DamagedHelmet.gltf" "D1A63F722956B44643D2592FB950E694160805B9")
DownloadFile("${gltf_sample_models_repo}2.0/DamagedHelmet/glTF/Default_albedo.jpg" "${asset_dir}DamagedHelmet/Default_albedo.jpg" "2FBA85A5A61993AEB13F238462BFA24272DE7A82")
DownloadFile("${gltf_sample_models_repo}2.0/DamagedHelmet/glTF/Default_AO.jpg" "${asset_dir}DamagedHelmet/Default_AO.jpg" "8B23E689CF6C9A03F2E7B257082773838010EB0F")
DownloadFile("${gltf_sample_models_repo}2.0/DamagedHelmet/glTF/Default_emissive.jpg" "${asset_dir}DamagedHelmet/Default_emissive.jpg" "57D9AAD5A1DBC2058F2CD0AC59B40722F316089F")
DownloadFile("${gltf_sample_models_repo}2.0/DamagedHelmet/glTF/Default_metalRoughness.jpg" "${asset_dir}DamagedHelmet/Default_metalRoughness.jpg" "51475318B7AD0F93E3C62A139788AD5691922728")
DownloadFile("${gltf_sample_models_repo}2.0/DamagedHelmet/glTF/Default_normal.jpg" "${asset_dir}DamagedHelmet/Default_normal.jpg" "8329E21A4CD6F6C06F77AD09F941863D00A508B3")

DownloadFile("${gltf_sample_models_repo}2.0/AlphaBlendModeTest/glTF/AlphaBlendLabels.png" "${asset_dir}AlphaBlendModeTest/AlphaBlendLabels.png" "94BBB5BDABDBF188EEDF8E61933EFE1BF6E9620E")
DownloadFile("${gltf_sample_models_repo}2.0/AlphaBlendModeTest/glTF/AlphaBlendModeTest.bin" "${asset_dir}AlphaBlendModeTest/AlphaBlendModeTest.bin" "9B7ABF94704E0AA42D0766559AA36FDD33A0559C")
DownloadFile("${gltf_sample_models_repo}2.0/AlphaBlendModeTest/glTF/AlphaBlendModeTest.gltf" "${asset_dir}AlphaBlendModeTest/AlphaBlendModeTest.gltf" "DAC70758179A3CAB5A12EA3917FFC27DB9A1DBA5")
DownloadFile("${gltf_sample_models_repo}2.0/AlphaBlendModeTest/glTF/MatBed_baseColor.jpg" "${asset_dir}AlphaBlendModeTest/MatBed_baseColor.jpg" "E609314BE2D6A4245E8A34C31FC7A8E8088B0A93")
DownloadFile("${gltf_sample_models_repo}2.0/AlphaBlendModeTest/glTF/MatBed_normal.jpg" "${asset_dir}AlphaBlendModeTest/MatBed_normal.jpg" "D9215BBAE6C830F8DC54FC71E9C92A6908F54489")
DownloadFile("${gltf_sample_models_repo}2.0/AlphaBlendModeTest/glTF/MatBed_occlusionRoughnessMetallic.jpg" "${asset_dir}AlphaBlendModeTest/MatBed_occlusionRoughnessMetallic.jpg" "12EDFC49C0984E478EC033597F92A5B8DB24D25C")

set(asset_deploy_dir "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/Assets/")
add_custom_target(CopyAssets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${asset_dir}" "${asset_deploy_dir}"
    COMMENT "Copying \"${asset_dir}\" to \"${asset_deploy_dir}\""
)

set_target_properties(CopyAssets PROPERTIES
    FOLDER "Samples"
)

add_subdirectory(App)
