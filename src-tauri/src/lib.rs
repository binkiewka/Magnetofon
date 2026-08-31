mod commands;

use commands::*;
use std::ffi::OsStr;
use std::path::{Path, PathBuf};
use tauri::{Emitter, Manager};

const AUDIO_EXTENSIONS: &[&str] = &[
    "aac", "aif", "aiff", "flac", "m4a", "m4b", "mp2", "mp3", "mpga", "oga", "ogg", "opus", "wav",
    "wma",
];

fn audio_files_from_args<I, S>(args: I, cwd: &Path) -> Vec<String>
where
    I: IntoIterator<Item = S>,
    S: AsRef<OsStr>,
{
    args.into_iter()
        .filter_map(|arg| {
            let path = PathBuf::from(arg.as_ref());
            let path = if path.is_absolute() {
                path
            } else {
                cwd.join(path)
            };
            let extension = path.extension()?.to_str()?.to_ascii_lowercase();

            if !AUDIO_EXTENSIONS.contains(&extension.as_str()) || !path.is_file() {
                return None;
            }

            Some(
                path.canonicalize()
                    .unwrap_or(path)
                    .to_string_lossy()
                    .into_owned(),
            )
        })
        .collect()
}

#[tauri::command]
fn get_launch_audio_files() -> Vec<String> {
    let cwd = std::env::current_dir().unwrap_or_default();
    audio_files_from_args(std::env::args_os().skip(1), &cwd)
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_single_instance::init(|app, argv, cwd| {
            let files = audio_files_from_args(argv.into_iter().skip(1), Path::new(&cwd));
            if !files.is_empty() {
                let _ = app.emit("open-audio-files", files);
            }

            if let Some(window) = app.get_webview_window("player") {
                let _ = window.show();
                let _ = window.unminimize();
                let _ = window.set_focus();
            }
        }))
        .setup(|app| {
            if let Some(window) = app.get_webview_window("player") {
                let icon = tauri::image::Image::from_bytes(include_bytes!("../icons/icon.png"))?;
                window.set_icon(icon)?;
            }
            Ok(())
        })
        .plugin(tauri_plugin_shell::init())
        .plugin(tauri_plugin_dialog::init())
        .plugin(tauri_plugin_fs::init())
        .plugin(tauri_plugin_clipboard_manager::init())
        .invoke_handler(tauri::generate_handler![
            get_preset_pack_status,
            download_preset_pack,
            get_projectm_status,
            launch_projectm,
            stop_projectm,
            toggle_projectm,
            toggle_window,
            get_launch_audio_files
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
