#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <memory>
#include "imgui.h"
#include "imgui-SFML.h"
#include "visualizer.h"
#include <optional>
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/flacfile.h>
#include <taglib/tfile.h>
#include <taglib/tfilestream.h>
#include <taglib/tbytevector.h>
#include <taglib/flacpicture.h> // Required for modern Windows dialogs
#include <iostream>
#include <algorithm>
#include <windows.h> // Required for conversion helper

#ifdef _WIN32

// Helper function to convert UTF-8 (std::string) to UTF-16 (std::wstring)
std::wstring utf8ToUtf16(const std::string& utf8Str) {
    if (utf8Str.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &utf8Str[0], (int)utf8Str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &utf8Str[0], (int)utf8Str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

std::wstring utf8ToUtf16(const std::string& utf8Str)
{
    return std::wstring(
        utf8Str.begin(),
        utf8Str.end()
    );
}

#endif


// Windows-specific headers to extract the embedded resource
#ifdef _WIN32
#include <windows.h>
#define IDB_PNG_ICON 101 // Must match the resource ID if we use integers, or we can look it up by string
#endif

namespace fs = std::filesystem;
const std::string CONFIG_FILE = "mish_config.txt";
const float AUTO_SCAN_INTERVAL = 120.0f; 

struct Track {
    std::string filename;
    std::string fullPath;
    
    // Extracted Metadata
    std::string title;
    std::string artist;
    std::string album;
    
    // Embedded Album Art
    std::shared_ptr<sf::Texture> albumArt = nullptr; 
};

// Helper function to load our embedded PNG resource from the .exe binary
bool loadIconFromResource(sf::Image& icon) {
#ifdef _WIN32
    // Find the custom "PNGFILE" resource named "IDB_PNG_ICON"
    HRSRC hResource = FindResourceA(NULL, "IDB_PNG_ICON", "PNGFILE");
    if (!hResource) return false;

    HGLOBAL hMemory = LoadResource(NULL, hResource);
    if (!hMemory) return false;

    DWORD dwSize = SizeofResource(NULL, hResource);
    LPVOID lpAddress = LockResource(hMemory);
    if (!lpAddress) return false;

    // Load SFML image directly from the extracted RAM buffer!
    return icon.loadFromMemory(lpAddress, dwSize);
#else
    return false;
#endif
}

std::string openFolderDialog() {
    std::string folderPath = "";
    
    // Initialize COM library
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (SUCCEEDED(hr)) {
        IFileOpenDialog *pDlg = NULL;
        
        // Create the FileOpenDialog object
        hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, 
                             IID_IFileOpenDialog, reinterpret_cast<void**>(&pDlg));
        
        if (SUCCEEDED(hr)) {
            // Set options to select folders instead of files
            DWORD dwOptions;
            pDlg->GetOptions(&dwOptions);
            pDlg->SetOptions(dwOptions | FOS_PICKFOLDERS);
            
            // Show the dialog
            hr = pDlg->Show(NULL);
            
            if (SUCCEEDED(hr)) {
                IShellItem *pItem = NULL;
                hr = pDlg->GetResult(&pItem);
                
                if (SUCCEEDED(hr)) {
                    PWSTR pszPath = NULL;
                    hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
                    
                    if (SUCCEEDED(hr)) {
                        // Convert Wide String (UTF-16) to UTF-8 std::string
                        int size_needed = WideCharToMultiByte(CP_UTF8, 0, pszPath, -1, NULL, 0, NULL, NULL);
                        if (size_needed > 0) {
                            folderPath.resize(size_needed - 1);
                            WideCharToMultiByte(CP_UTF8, 0, pszPath, -1, &folderPath[0], size_needed, NULL, NULL);
                        }
                        CoTaskMemFree(pszPath);
                    }
                    pItem->Release();
                }
            }
            pDlg->Release();
        }
        CoUninitialize();
    }
    return folderPath;
}

void extractMetadata(Track& track) {
    std::cout << "\n[TagLib] Reading: " << track.filename << std::endl;

    // Defaults
    track.title = track.filename;
    track.artist = "Unknown Artist";
    track.album = "Unknown Album";
    track.albumArt = nullptr;


    // ----------------------------
    // Read normal metadata
    // ----------------------------

    TagLib::FileRef f(track.fullPath.c_str());
	
	std::cout << "Opening path: " << track.fullPath << "\n";
	std::cout << "Exists: "
          << std::filesystem::exists(track.fullPath)
          << "\n";

    if (!f.isNull() && f.tag()) {
        TagLib::Tag* tag = f.tag();

        if (!tag->title().isEmpty())
            track.title = tag->title().to8Bit(true);

        if (!tag->artist().isEmpty())
            track.artist = tag->artist().to8Bit(true);

        if (!tag->album().isEmpty())
            track.album = tag->album().to8Bit(true);


        std::cout
            << "   -> Title:  " << track.title
            << "\n   -> Artist: " << track.artist
            << "\n   -> Album:  " << track.album
            << "\n";
    }
    else {
        std::cout << "   -> Failed reading basic tags\n";
    }


    // ----------------------------
    // Extension
    // ----------------------------

    size_t dotPos = track.fullPath.find_last_of('.');

    if (dotPos == std::string::npos)
        return;


    std::string ext = track.fullPath.substr(dotPos + 1);

    std::transform(
        ext.begin(),
        ext.end(),
        ext.begin(),
        [](unsigned char c)
        {
            return std::tolower(c);
        }
    );


    // ----------------------------
    // MP3 album art
    // ----------------------------

    if (ext == "mp3") {

    if (f.isNull()) {
        std::cout << "   No TagLib file handle available\n";
        return;
    }

    TagLib::File* baseFile = f.file();

    auto* mpegFile =
        dynamic_cast<TagLib::MPEG::File*>(baseFile);


    if (!mpegFile) {
        std::cout << "   Dynamic cast to MPEG failed\n";
        return;
    }


    std::cout << "   MPEG cast OK!\n";


    auto* id3 = mpegFile->ID3v2Tag();

    if (!id3) {
        std::cout << "   No ID3v2 tag found\n";
        return;
    }


    auto frames = id3->frameList();

    std::cout
        << "   Total ID3 frames: "
        << frames.size()
        << "\n";


    for (auto frame : frames) {

        std::cout
            << "   Frame: "
            << frame->frameID().data()
            << "\n";


        if (frame->frameID() == TagLib::ByteVector("APIC")) {

            auto* picture =
                dynamic_cast<TagLib::ID3v2::AttachedPictureFrame*>(frame);


            if (!picture) {
                std::cout << "   APIC cast failed\n";
                continue;
            }


            auto data = picture->picture();

            std::cout
                << "   MIME: "
                << picture->mimeType().to8Bit(true)
                << "\n";

            std::cout
                << "   Image bytes: "
                << data.size()
                << "\n";


            auto texture =
                std::make_shared<sf::Texture>();


            if(texture->loadFromMemory(data.data(), data.size()))
            {
                track.albumArt = texture;
                std::cout << "   SFML loaded album art!\n";
                return;
            }
        }
    }


        std::cout
            << "   No APIC frame found\n";
    }


    // ----------------------------
    // FLAC art
    // ----------------------------

		else if (ext == "flac") {

		if (f.isNull()) {
			std::cout << "   No TagLib file handle available\n";
			return;
		}

		TagLib::File* baseFile = f.file();

		auto* flacFile =
			dynamic_cast<TagLib::FLAC::File*>(baseFile);

		if (!flacFile) {
			std::cout << "   Dynamic cast to FLAC failed\n";
			return;
		}

		std::cout << "   FLAC cast OK!\n";


		auto pictures = flacFile->pictureList();

		std::cout << "   FLAC pictures found: "
				  << pictures.size()
				  << "\n";


		if (!pictures.isEmpty())
		{
			TagLib::FLAC::Picture* cover = nullptr;

			for (auto* pic : pictures)
			{
				if (pic->type() == TagLib::FLAC::Picture::FrontCover)
				{
					cover = pic;
					break;
				}
			}

			if (!cover)
				cover = pictures.front();


			auto data = cover->data();

			std::cout
				<< "   FLAC art bytes: "
				<< data.size()
				<< "\n";


			auto texture =
				std::make_shared<sf::Texture>();


			if(texture->loadFromMemory(
					data.data(),
					data.size()))
			{
				track.albumArt = texture;

				std::cout
					<< "   FLAC art loaded!\n";
			}
			else
			{
				std::cout
					<< "   FLAC art decode failed!\n";
			}
		}
	}
}

void saveConfig(const std::string& path) {
    std::ofstream cfg(CONFIG_FILE);
    if (cfg.is_open()) { cfg << path; cfg.close(); }
}

std::string loadConfig() {
    std::ifstream cfg(CONFIG_FILE);
    if (cfg.is_open()) {
        std::string path;
        std::getline(cfg, path);
        cfg.close();
        return path;
    }
    return "";
}

std::vector<Track> scanMusicFolder(const std::string& folderPath) {
    std::vector<Track> playlist;
    if (!fs::exists(folderPath) || !fs::is_directory(folderPath)) return playlist;
    for (const auto& entry : fs::directory_iterator(folderPath)) {
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            for (char &c : ext) c = std::tolower(c);
            if (ext == ".mp3" || ext == ".ogg" || ext == ".wav" || ext == ".flac") {
                // 1. Add the basic track to the playlist
                playlist.push_back({entry.path().filename().string(), entry.path().string()});
                
                // 2. Extract metadata using the last item in the vector
                extractMetadata(playlist.back());
            }
        }
    }
    return playlist;
}
int main() {
    sf::RenderWindow window(sf::VideoMode({1024, 768}), "mish");
    window.setFramerateLimit(60);

    // Center the window on the desktop monitor screen
    sf::VideoMode desktop = sf::VideoMode::getDesktopMode();
    window.setPosition(sf::Vector2i(
        static_cast<int>((desktop.size.x - window.getSize().x) / 2),
        static_cast<int>((desktop.size.y - window.getSize().y) / 2)
    ));

    // --- LOAD BUNDLED ICON DIRECTLY FROM EXECUTABLE MEMORY ---
    sf::Image icon;
    if (loadIconFromResource(icon)) {
        window.setIcon(icon.getSize(), icon.getPixelsPtr());
    } else {
        std::cerr << "Warning: Could not load bundled PNG icon from resources\n";
    }

    if (!ImGui::SFML::Init(window)) {
        std::cerr << "Failed to initialize ImGui-SFML\n";
        return -1;
    }

    sf::SoundBuffer buffer;
    std::optional<sf::Sound> sound; 
    Visualizer visualizer(1024, 768);
    
    bool setupDone = false;
    char pathBuffer[256] = "";
    std::string savedPath = loadConfig();
    std::vector<Track> tracks;
    int currentTrackIndex = -1;
	
	enum class RepeatMode {
		Off,
		Track,
		Playlist
	};

	RepeatMode repeatMode = RepeatMode::Off;

    if (!savedPath.empty() && fs::exists(savedPath) && fs::is_directory(savedPath)) {
        tracks = scanMusicFolder(savedPath);
        if (!tracks.empty()) {
            currentTrackIndex = 0;
            if (buffer.loadFromFile(tracks[currentTrackIndex].fullPath)) {
                sound.emplace(buffer); 
                setupDone = true;
            }
        }
    }

    sf::Clock deltaClock;
    sf::Clock autoScanClock;

    while (window.isOpen()) {
        while (const std::optional event = window.pollEvent()) {
            ImGui::SFML::ProcessEvent(window, *event);
            if (event->is<sf::Event::Closed>()) window.close();
        }

        // Auto rescan
        if (setupDone && autoScanClock.getElapsedTime().asSeconds() >= AUTO_SCAN_INTERVAL) {
            autoScanClock.restart();
            std::string currentPlayingPath = (currentTrackIndex != -1) ? tracks[currentTrackIndex].fullPath : "";
            tracks = scanMusicFolder(savedPath);
            currentTrackIndex = -1;
            for (int i = 0; i < (int)tracks.size(); ++i) {
                if (tracks[i].fullPath == currentPlayingPath) { currentTrackIndex = i; break; }
            }
        }

        ImGui::SFML::Update(window, deltaClock.restart());

        if (sound.has_value()) {
            visualizer.update(*sound, buffer);
        }
		// Automatic song advancing
		if (sound.has_value() &&
			sound->getStatus() == sf::Sound::Status::Stopped &&
			currentTrackIndex != -1)
		{

			if (repeatMode == RepeatMode::Track) {

				sound->play();
			}

			else if (repeatMode == RepeatMode::Playlist) {

				currentTrackIndex++;

				if (currentTrackIndex >= (int)tracks.size()) {
					currentTrackIndex = 0;
				}

				if (buffer.loadFromFile(tracks[currentTrackIndex].fullPath)) {

					sound.emplace(buffer);
					sound->play();
				}
			}

			else {

				// Normal mode
				if (currentTrackIndex < (int)tracks.size() - 1) {

					currentTrackIndex++;

					if (buffer.loadFromFile(tracks[currentTrackIndex].fullPath)) {

						sound.emplace(buffer);
						sound->play();
					}

				}
			}
		}
		

        if (!setupDone) {
			ImGui::Begin("first-time setup", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
			ImGui::SetWindowSize(ImVec2(500, 300));
			ImGui::Text("mish Setup");
			ImGui::Separator();
			ImGui::Spacing();

			// 1. Display the selected path (Read-only so they don't have to type)
			if (pathBuffer[0] == '\0') {
				ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "selected Path: [no folder selected]");
			} else {
				ImGui::Text("selected path: %s", pathBuffer);
			}
			ImGui::Spacing();

			// 2. The Browse button that opens the native Windows Explorer window
			if (ImGui::Button("browse folder...")) {
				std::string chosenPath = openFolderDialog();
				if (!chosenPath.empty()) {
					// Copy the selected path into your pathBuffer securely
					strncpy(pathBuffer, chosenPath.c_str(), sizeof(pathBuffer) - 1);
					pathBuffer[sizeof(pathBuffer) - 1] = '\0';
				}
			}
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			// Disable the save button if they haven't picked a folder yet
			bool hasPath = (pathBuffer[0] != '\0');
			if (!hasPath) {
				ImGui::BeginDisabled();
			}

			// 3. Your original Save Path / Confirm button logic
			if (ImGui::Button("Save Path")) {
				std::string pathStr(pathBuffer);
				tracks = scanMusicFolder(pathStr);
				if (tracks.empty()) {
					ImGui::OpenPopup("Scan Error");
				} else {
					savedPath = pathStr;
					saveConfig(savedPath);
					currentTrackIndex = 0;
					if (buffer.loadFromFile(tracks[currentTrackIndex].fullPath)) {
						sound.emplace(buffer); 
						setupDone = true; 
						autoScanClock.restart();
					}
				}
			}

			if (!hasPath) {
				ImGui::EndDisabled();
			}

			// Your original popup modal code remains exactly where it was
			if (ImGui::BeginPopupModal("scan Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
				ImGui::Text("no files found!");
				if (ImGui::Button("OK")) { ImGui::CloseCurrentPopup(); }
				ImGui::EndPopup();
			}

			ImGui::End();
			} else {
				ImGui::SetNextWindowPos(
				ImVec2(10, 10),
				ImGuiCond_Always
			);
			ImGui::SetNextWindowSize(
				ImVec2(500, 300),
				ImGuiCond_Always
			);
			ImGui::Begin(
				"mish player"
			);
            if (currentTrackIndex >= 0 && currentTrackIndex < (int)tracks.size()) {
                ImGui::Text("now playing: %s", tracks[currentTrackIndex].filename.c_str());
            }
            float timeUntilScan = AUTO_SCAN_INTERVAL - autoScanClock.getElapsedTime().asSeconds();
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Auto-sync in: %.0fs", timeUntilScan);
            ImGui::Separator();

            bool isPlaying = (sound.has_value() && sound->getStatus() == sf::Sound::Status::Playing);

            if (!isPlaying) {
                if (ImGui::Button("play") && currentTrackIndex != -1) {
                    if (sound.has_value()) sound->play();
                }
            } else {
                if (ImGui::Button("pause")) {
                    if (sound.has_value()) sound->pause();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("stop")) {
                if (sound.has_value()) sound->stop();
            }
            ImGui::SameLine();
            if (ImGui::Button("next")) {
                if (!tracks.empty() && currentTrackIndex < (int)tracks.size() - 1) {
                    currentTrackIndex++;
                    if (sound.has_value()) sound->stop();
                    if (buffer.loadFromFile(tracks[currentTrackIndex].fullPath)) {
                        sound.emplace(buffer);
						sound->setPlayingOffset(sf::seconds(0));
						sound->play();
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("prev")) {
                if (!tracks.empty() && currentTrackIndex > 0) {
                    currentTrackIndex--;
                    if (sound.has_value()) sound->stop();
                    if (buffer.loadFromFile(tracks[currentTrackIndex].fullPath)) {
						sound.emplace(buffer);
						sound->setPlayingOffset(sf::seconds(0));
						sound->play();
                    }
                }
            }
			ImGui::SameLine();
			
			if (ImGui::Button("Force Scan")) {
				std::string currentPlayingPath = "";

				if (currentTrackIndex >= 0 &&
					currentTrackIndex < (int)tracks.size()) {
					currentPlayingPath = tracks[currentTrackIndex].fullPath;
				}

				tracks = scanMusicFolder(savedPath);

				currentTrackIndex = -1;

				// Try to keep the currently playing song selected
				for (int i = 0; i < (int)tracks.size(); i++) {
					if (tracks[i].fullPath == currentPlayingPath) {
						currentTrackIndex = i;
						break;
					}
				}

				autoScanClock.restart();

				std::cout << "Force scan complete! Found "
						  << tracks.size()
						  << " tracks.\n";
			}

			const char* repeatText = "";

			switch (repeatMode) {
				case RepeatMode::Off:
					repeatText = "Repeat Off";
					break;

				case RepeatMode::Track:
					repeatText = "Repeat One";
					break;

				case RepeatMode::Playlist:
					repeatText = "Repeat All";
					break;
			}

			if (ImGui::Button(repeatText)) {

				if (repeatMode == RepeatMode::Off) {
					repeatMode = RepeatMode::Track;
				}
				else if (repeatMode == RepeatMode::Track) {
					repeatMode = RepeatMode::Playlist;
				}
				else {
					repeatMode = RepeatMode::Off;
				}
			}
			// --- SEEK BAR ---
			if (sound.has_value() && currentTrackIndex >= 0) {

				float duration = buffer.getDuration().asSeconds();
				float current = sound->getPlayingOffset().asSeconds();

				if (duration > 0) {

					ImGui::Text(
						"%02d:%02d / %02d:%02d",
						(int)(current / 60),
						(int)current % 60,
						(int)(duration / 60),
						(int)duration % 60
					);

					float seek = current;

					if (ImGui::SliderFloat(
						"##seek",
						&seek,
						0.0f,
						duration,
						""
					)) {
						sound->setPlayingOffset(sf::seconds(seek));
					}
				}
			}
            ImGui::Spacing();
            ImGui::Separator();
			
            
            ImGui::BeginChild("TrackList", ImVec2(0, 150), true);
            for (int i = 0; i < (int)tracks.size(); ++i) {
                bool isSelected = (i == currentTrackIndex);
                if (ImGui::Selectable(tracks[i].filename.c_str(), isSelected)) {
                    currentTrackIndex = i;
                    if (sound.has_value()) sound->stop();
                    if (buffer.loadFromFile(tracks[currentTrackIndex].fullPath)) {
                        sound.emplace(buffer); 
                        sound->play();
                    }
                }
            }
            ImGui::EndChild();

            if (ImGui::Button("change music folder")) {
                if (sound.has_value()) sound->stop();
                setupDone = false; 
                tracks.clear();
                currentTrackIndex = -1;
                sound.reset(); 
                snprintf(pathBuffer, sizeof(pathBuffer), "%s", savedPath.c_str());
            }
            ImGui::End();
        }
			if (setupDone && !tracks.empty()) {
		// Access the current track metadata
		const Track& currentTrack = tracks[currentTrackIndex];

		// Set a sleek style for our Now Playing Card
		ImGui::SetNextWindowPos(
			ImVec2(10, 320),
			ImGuiCond_Always
		);

		ImGui::SetNextWindowSize(
			ImVec2(350, 250),
			ImGuiCond_Always
		);

		ImGui::Begin("song info");		
		// --- ALBUM ART CONTAINER ---
		if (currentTrack.albumArt) {
			// Display the loaded dynamic texture
			// (ImTextureID conversion varies slightly depending on your ImGui-SFML backend)
			ImGui::Image(*currentTrack.albumArt, sf::Vector2f(150.f, 150.f)); //  Correct!
		} else {
			// Draw a nice fallback placeholder if there's no album cover
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			ImVec2 p = ImGui::GetCursorScreenPos();
			
			// Draw a dark gray rectangle
			drawList->AddRectFilled(p, ImVec2(p.x + 150, p.y + 150), IM_COL32(45, 45, 45, 255), 8.0f);
			// Draw a music note icon representation/text in the center
			ImGui::SetCursorScreenPos(ImVec2(p.x + 40, p.y + 65));
			ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "[ NO ART ]");
			
			// Restore cursor position for next elements
			ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + 150));
		}
		
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// --- TEXT METADATA ---
		// Make the title big and highlighted
		ImGui::TextColored(ImVec4(0.0f, 0.9f, 1.0f, 1.0f), "%s", currentTrack.title.c_str());
		
		// Subtext for Artist & Album
		ImGui::Text("Artist: %s", currentTrack.artist.c_str());
		ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "album:  %s", currentTrack.album.c_str());

		ImGui::End();
	}

        window.clear(sf::Color(10, 10, 12));
        
        visualizer.draw(window);
        
        ImGui::SFML::Render(window);
        window.display();
    }

    ImGui::SFML::Shutdown();
    return 0;
}