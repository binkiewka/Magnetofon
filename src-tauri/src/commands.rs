use serde::{Deserialize, Serialize};
use std::fs::{self, File};
use std::io::{self, Write};
use std::path::{Path, PathBuf};
use std::process::{Child, Command};
use std::sync::Mutex;
use tauri::{AppHandle, Emitter, Manager};

lazy_static::lazy_static! {
    static ref PROJECTM_PROCESS: Mutex<Option<Child>> = Mutex::new(None);
}

#[derive(Debug, Serialize, Deserialize)]
pub struct PresetPackStatus {
    pub is_installed: bool,
    pub count: usize,
    pub path: String,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct ProjectMConfig {
    pub preset_category: Option<String>,
    pub preset_path: Option<String>,
    pub fps: Option<u32>,
    pub width: Option<u32>,
    pub height: Option<u32>,
    pub beat_sensitivity: Option<f32>,
}

fn get_visuals_dir(app_handle: &AppHandle) -> PathBuf {
    if let Ok(app_data) = app_handle.path().app_data_dir() {
        let user_visuals = app_data.join("visuals");
        if user_visuals.join("presets").exists() {
            return user_visuals;
        }
    }
    let local_visuals = PathBuf::from("visuals");
    if local_visuals.join("presets").exists() {
        return local_visuals;
    }
    app_handle.path().app_data_dir().unwrap_or_default().join("visuals")
}

fn count_milk_files(dir: &Path) -> usize {
    let mut count = 0;
    if let Ok(entries) = fs::read_dir(dir) {
        for entry in entries.flatten() {
            let path = entry.path();
            if path.is_dir() {
                count += count_milk_files(&path);
            } else if path.extension().and_then(|s| s.to_str()) == Some("milk") {
                count += 1;
            }
        }
    }
    count
}

#[tauri::command]
pub async fn get_preset_pack_status(app_handle: AppHandle) -> Result<PresetPackStatus, String> {
    let visuals_dir = get_visuals_dir(&app_handle);
    let presets_dir = visuals_dir.join("presets");
    
    if presets_dir.exists() {
        let count = count_milk_files(&presets_dir);
        Ok(PresetPackStatus {
            is_installed: count > 100,
            count,
            path: presets_dir.to_string_lossy().to_string(),
        })
    } else {
        Ok(PresetPackStatus {
            is_installed: false,
            count: 0,
            path: "".to_string(),
        })
    }
}

#[tauri::command]
pub async fn download_preset_pack(app_handle: AppHandle) -> Result<PresetPackStatus, String> {
    let target_dir = app_handle
        .path()
        .app_data_dir()
        .map_err(|e| e.to_string())?
        .join("visuals");

    fs::create_dir_all(&target_dir).map_err(|e| e.to_string())?;
    let zip_path = target_dir.join("preset_pack.zip");

    let url = "https://github.com/binkiewka/Magnetofon/releases/download/v1.0.0/Isosceles_CreamOfTheCrop_MilkdropPresetsPack.zip";
    
    let client = reqwest::Client::new();
    let res = client.get(url).send().await.map_err(|e| e.to_string())?;
    
    let total_size = res.content_length().unwrap_or(138_000_000);
    let mut downloaded: u64 = 0;
    let mut stream = res.bytes_stream();
    
    let mut out_file = File::create(&zip_path).map_err(|e| e.to_string())?;

    use futures_util::StreamExt;
    while let Some(chunk_result) = stream.next().await {
        let chunk = chunk_result.map_err(|e| e.to_string())?;
        out_file.write_all(&chunk).map_err(|e| e.to_string())?;
        downloaded += chunk.len() as u64;
        
        let percent = ((downloaded as f64 / total_size as f64) * 50.0) as u32;
        let _ = app_handle.emit("preset-pack-progress", serde_json::json!({
            "stage": "downloading",
            "percent": percent,
            "downloaded": downloaded,
            "total": total_size
        }));
    }

    let _ = app_handle.emit("preset-pack-progress", serde_json::json!({
        "stage": "extracting",
        "percent": 55,
    }));

    let zip_file = File::open(&zip_path).map_err(|e| e.to_string())?;
    let mut archive = zip::ZipArchive::new(zip_file).map_err(|e| e.to_string())?;
    
    let len = archive.len();
    for i in 0..len {
        let mut file = archive.by_index(i).map_err(|e| e.to_string())?;
        let outpath = match file.enclosed_name() {
            Some(path) => target_dir.join(path),
            None => continue,
        };

        if file.is_dir() {
            fs::create_dir_all(&outpath).map_err(|e| e.to_string())?;
        } else {
            if let Some(p) = outpath.parent() {
                if !p.exists() {
                    fs::create_dir_all(p).map_err(|e| e.to_string())?;
                }
            }
            let mut outfile = File::create(&outpath).map_err(|e| e.to_string())?;
            io::copy(&mut file, &mut outfile).map_err(|e| e.to_string())?;
        }

        if i % 100 == 0 {
            let percent = 50 + (((i as f64 / len as f64) * 50.0) as u32);
            let _ = app_handle.emit("preset-pack-progress", serde_json::json!({
                "stage": "extracting",
                "percent": percent
            }));
        }
    }

    let _ = fs::remove_file(zip_path);

    let count = count_milk_files(&target_dir.join("presets"));
    let status = PresetPackStatus {
        is_installed: count > 100,
        count,
        path: target_dir.join("presets").to_string_lossy().to_string(),
    };

    let _ = app_handle.emit("preset-pack-progress", serde_json::json!({
        "stage": "completed",
        "percent": 100,
        "count": count
    }));

    Ok(status)
}

#[tauri::command]
pub async fn launch_projectm(app_handle: AppHandle, config: Option<ProjectMConfig>) -> Result<(), String> {
    stop_projectm().await?;

    let visuals_dir = get_visuals_dir(&app_handle);
    let mut preset_path = visuals_dir.join("presets");

    if let Some(cfg) = &config {
        if let Some(cat) = &cfg.preset_category {
            let sub = match cat.to_uppercase().as_str() {
                "DANCER" => "Presets/Dancer",
                "DRAWING" => "Presets/Drawing",
                "FRACTAL" => "Presets/Fractal",
                "GEOMETRIC" => "Presets/Geometric",
                "HYPNOTIC" => "Presets/Hypnotic",
                "PARTICLES" => "Presets/Particles",
                "REACTION" => "Presets/Reaction",
                "SPARKLE" => "Presets/Sparkle",
                "SUPERNOVA" => "Presets/Supernova",
                "WAVEFORM" => "Presets/Waveform",
                _ => "presets",
            };
            let cat_dir = visuals_dir.join(sub);
            if cat_dir.exists() {
                preset_path = cat_dir;
            }
        }
    }

    let projectm_bin = if cfg!(target_os = "windows") {
        PathBuf::from("resources/projectm/win-x64/bin/projectMSDL.exe")
    } else {
        PathBuf::from("resources/projectm/linux-x64/bin/projectMSDL")
    };

    let mut cmd = Command::new(projectm_bin);
    cmd.arg("--presetPath").arg(&preset_path);
    cmd.arg("--texturePath").arg(visuals_dir.join("textures"));

    if let Some(cfg) = config {
        if let Some(fps) = cfg.fps {
            cmd.arg("--fps").arg(fps.to_string());
        }
        if let Some(w) = cfg.width {
            cmd.arg("--width").arg(w.to_string());
        }
        if let Some(h) = cfg.height {
            cmd.arg("--height").arg(h.to_string());
        }
    }

    match cmd.spawn() {
        Ok(child) => {
            let mut lock = PROJECTM_PROCESS.lock().unwrap();
            *lock = Some(child);
            Ok(())
        }
        Err(err) => Err(format!("Failed to launch projectM: {}", err)),
    }
}

#[tauri::command]
pub async fn stop_projectm() -> Result<(), String> {
    let mut lock = PROJECTM_PROCESS.lock().unwrap();
    if let Some(mut child) = lock.take() {
        let _ = child.kill();
    }
    Ok(())
}

#[tauri::command]
pub async fn toggle_window(app_handle: AppHandle, label: String) -> Result<(), String> {
    if let Some(win) = app_handle.get_webview_window(&label) {
        if win.is_visible().unwrap_or(false) {
            let _ = win.hide();
        } else {
            let _ = win.show();
            let _ = win.set_focus();
        }
    }
    Ok(())
}
