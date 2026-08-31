mod commands;

use commands::*;

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_shell::init())
        .plugin(tauri_plugin_dialog::init())
        .plugin(tauri_plugin_fs::init())
        .plugin(tauri_plugin_clipboard_manager::init())
        .invoke_handler(tauri::generate_handler![
            get_preset_pack_status,
            download_preset_pack,
            launch_projectm,
            stop_projectm,
            toggle_window
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
