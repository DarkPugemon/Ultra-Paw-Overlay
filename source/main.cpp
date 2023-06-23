#define NDEBUG
#define STBTT_STATIC
#define TESLA_INIT_IMPL
#include <tesla.hpp>
#include <utils.hpp>
#include <sys/stat.h>


// Overlay booleans
bool inMainMenu = false;
bool inSubMenu = false;
bool inConfigMenu = false;
bool inSelectionMenu = false;

// Helper function to handle overlay menu input
bool handleOverlayMenuInput(bool& inMenu, u64 keysHeld, u64 backKey, uint64_t sleepTime = 300'000'000) {
    if (inMenu && (keysHeld & backKey)) {
        svcSleepThread(sleepTime);
        inMenu = false;
        tsl::goBack();
        return true;
    }
    return false;
}

// Selection overlay helper function (for toggles too)
std::vector<std::vector<std::string>> getModifyCommands(const std::vector<std::vector<std::string>>& commands, const std::string& file, bool toggle=false, bool on=true) {
    std::vector<std::vector<std::string>> modifiedCommands;
    bool addCommands = false;
    for (const auto& cmd : commands) {
        if (toggle) {
            if (cmd.size() > 1 && cmd[0] == "source_on") {
                addCommands = true;
                if (!on) {
                    addCommands = !addCommands;
                }
            } else if (cmd.size() > 1 && cmd[0] == "source_off") {
                addCommands = false;
                if (!on) {
                    addCommands = !addCommands;
                }
            }
        }
        
        if (!toggle or addCommands) {
            std::vector<std::string> modifiedCmd = cmd;
            for (auto& arg : modifiedCmd) {
                if ((!toggle && arg == "{source}") || (on && arg == "{source_on}") || (!on && arg == "{source_off}")) {
                    arg = file;
                }
            }
            modifiedCommands.emplace_back(modifiedCmd);
        }
    }
    return modifiedCommands;
}


// Config overlay 
class ConfigOverlay : public tsl::Gui {
private:
    std::string filePath;
    std::string specificKey;

public:
    ConfigOverlay(const std::string& file, const std::string& key = "") : filePath(file), specificKey(key) {}

    virtual tsl::elm::Element* createUI() override {
        inConfigMenu = true;
        
        auto rootFrame = new tsl::elm::OverlayFrame(getNameFromPath(filePath), "Ultrahand Config");
        auto list = new tsl::elm::List();

        std::string configFile = filePath + "/config.ini";

        std::string fileContent = readFileContent(configFile);
        if (!fileContent.empty()) {
            std::string line;
            std::istringstream iss(fileContent);
            std::string currentCategory;
            bool isInSection = false;
            while (std::getline(iss, line)) {
                if (line.empty() || line.find_first_not_of('\n') == std::string::npos) {
                    continue;
                }

                if (line.front() == '[' && line.back() == ']') {
                    if (!specificKey.empty()) {
                        if (line.substr(1, line.size() - 2) == specificKey) {
                            currentCategory = line.substr(1, line.size() - 2);
                            isInSection = true;
                            list->addItem(new tsl::elm::CategoryHeader(line.substr(1, line.size() - 2)));
                        } else {
                            currentCategory.clear();
                            isInSection = false;
                        }
                    } else {
                        currentCategory = line.substr(1, line.size() - 2);
                        isInSection = true;
                        list->addItem(new tsl::elm::CategoryHeader(line.substr(1, line.size() - 2)));
                    }
                } else {
                    if (isInSection) {
                        auto listItem = new tsl::elm::ListItem(line);
                        listItem->setClickListener([line, this](uint64_t keys) {
                            if (keys & KEY_A) {
                                std::istringstream iss(line);
                                std::vector<std::string> commandParts;
                                std::string part;
                                bool inQuotes = false;
                                while (std::getline(iss, part, '\'')) {
                                    if (!part.empty()) {
                                        if (!inQuotes) {
                                            std::istringstream argIss(part);
                                            std::string arg;
                                            while (argIss >> arg) {
                                                commandParts.emplace_back(arg);
                                            }
                                        } else {
                                            commandParts.emplace_back(part);
                                        }
                                    }
                                    inQuotes = !inQuotes;
                                }
                                std::string commandName = commandParts[0];
                                std::vector<std::vector<std::string>> commandVec;
                                commandVec.emplace_back(commandParts);
                                interpretAndExecuteCommand(commandVec);
                                return true;
                            }
                            return false;
                        });
                        list->addItem(listItem);
                    }
                }
            }
        } else {
            list->addItem(new tsl::elm::ListItem("Failed to open file: " + configFile));
        }

        rootFrame->setContent(list);
        return rootFrame;
    }

    virtual bool handleInput(u64 keysDown, u64 keysHeld, touchPosition touchInput, JoystickPosition leftJoyStick, JoystickPosition rightJoyStick) override {
        return handleOverlayMenuInput(inConfigMenu, keysHeld, KEY_B);
    }
};



// Selection overlay
class SelectionOverlay : public tsl::Gui {
private:
    std::string filePath;
    std::string specificKey;
    std::vector<std::vector<std::string>> commands;
    std::string filterPath;
    std::string pathPattern;
    std::string pathPatternOn;
    std::string pathPatternOff;
    std::vector<std::string> filesList;
    std::vector<std::string> filesListOn;
    std::vector<std::string> filesListOff;
    bool toggleState = false;

public:
    SelectionOverlay(const std::string& file, const std::string& key = "", const std::vector<std::vector<std::string>>& cmds = {}) 
        : filePath(file), specificKey(key), commands(cmds) {}

    virtual tsl::elm::Element* createUI() override {
        inSelectionMenu = true;

        auto rootFrame = new tsl::elm::OverlayFrame(getNameFromPath(filePath), "Ultrahand Package");
        auto list = new tsl::elm::List();

        list->addItem(new tsl::elm::CategoryHeader(specificKey.substr(1)));

        // Extract the path pattern from commands
        bool useToggle = false;

        for (const auto& cmd : commands) {
            if (cmd.size() > 1) {
                if (cmd[0] == "filter") {
                    filterPath = cmd[1];
                } else if (cmd[0] == "source") {
                    pathPattern = cmd[1];
                    break;
                } else if (cmd[0] == "source_on") {
                    pathPatternOn = cmd[1];
                    useToggle = true;
                } else if (cmd[0] == "source_off") {
                    pathPatternOff = cmd[1];
                    break;
                }
            } 
        }

        // Get the list of files matching the pattern
        //std::vector<std::string> filesList;
        //std::vector<std::string> filesListOn;
        //std::vector<std::string> filesListOff;
        
        if (!useToggle) {
            filesList = getFilesListByWildcards(pathPattern);
        } else {
            filesListOn = getFilesListByWildcards(pathPatternOn);
            filesListOff = getFilesListByWildcards(pathPatternOff);
            
            filesList.reserve(filesListOn.size() + filesListOff.size());
            filesList.insert(filesList.end(), filesListOn.begin(), filesListOn.end());
            filesList.insert(filesList.end(), filesListOff.begin(), filesListOff.end());

            std::sort(filesList.begin(), filesList.end(), [](const std::string& a, const std::string& b) {
                return getNameFromPath(a) < getNameFromPath(b);
            });
        }

        // Add each file as a menu item
        for (const std::string& file : filesList) {
            if (file != filterPath){
                if (!useToggle) {
                    auto listItem = new tsl::elm::ListItem(dropExtension(getNameFromPath(file)));
                    listItem->setClickListener([file, this](uint64_t keys) { // Add 'command' to the capture list
                        if (keys & KEY_A) {
                            // Replace "{source}" with file in commands, then execute
                            std::vector<std::vector<std::string>> modifiedCommands = getModifyCommands(commands, file);
                            interpretAndExecuteCommand(modifiedCommands);
                            return true;
                        }
                        return false;
                    });
                    list->addItem(listItem);
                } else { // for handiling toggles
                    auto toggleListItem = new tsl::elm::ToggleListItem(dropExtension(getNameFromPath(file)), false, "On", "Off");

                    // Set the initial state of the toggle item
                    bool toggleStateOn = std::find(filesListOn.begin(), filesListOn.end(), file) != filesListOn.end();
                    toggleListItem->setState(toggleStateOn);

                    toggleListItem->setStateChangedListener([toggleListItem, file, toggleStateOn, this](bool state) {
                        if (!state) {
                            // Toggle switched to On
                            if (toggleStateOn) {
                                std::vector<std::vector<std::string>> modifiedCommands = getModifyCommands(commands, file, true);
                                interpretAndExecuteCommand(modifiedCommands);
                            } else {
                                // Handle the case where the command should only run in the source_on section
                                // Add your specific code here
                            }
                        } else {
                            // Toggle switched to Off
                            if (!toggleStateOn) {
                                std::vector<std::vector<std::string>> modifiedCommands = getModifyCommands(commands, file, true, false);
                                interpretAndExecuteCommand(modifiedCommands);
                            } else {
                                // Handle the case where the command should only run in the source_off section
                                // Add your specific code here
                            }
                        }
                    });

                    list->addItem(toggleListItem);
                }
            }

        }

        rootFrame->setContent(list);
        return rootFrame;
    }

    virtual bool handleInput(u64 keysDown, u64 keysHeld, touchPosition touchInput, JoystickPosition leftJoyStick, JoystickPosition rightJoyStick) override {
        return handleOverlayMenuInput(inSelectionMenu, keysHeld, KEY_B);
    }
};



// Sub menu
class SubMenu : public tsl::Gui {
private:
    std::string subPath;
    std::string pathReplace;
    std::string pathReplaceOn;
    std::string pathReplaceOff;

public:
    SubMenu(const std::string& path) : subPath(path) {}

    virtual tsl::elm::Element* createUI() override {
        inSubMenu = true;
        
        auto rootFrame = new tsl::elm::OverlayFrame(getNameFromPath(subPath), "Ultrahand Package");
        auto list = new tsl::elm::List();

        // Add a section break with small text to indicate the "Commands" section
        list->addItem(new tsl::elm::CategoryHeader("Commands"));

        // Load options from INI file in the subdirectory
        std::string subConfigIniPath = subPath + "/config.ini";
        std::vector<std::pair<std::string, std::vector<std::vector<std::string>>>> options = loadOptionsFromIni(subConfigIniPath);
        
        // Populate the sub menu with options
        for (const auto& option : options) {
            std::string optionName = option.first;
            std::string footer; 
            bool usePattern = false;
            if (optionName[0] == '*') { 
                usePattern = true;
                optionName = optionName.substr(1); // Strip the "*" character on the left
                footer = ">";
            }
            
            // Extract the path pattern from commands
            bool useToggle = false;
            for (const auto& cmd : option.second) {
                if (cmd.size() > 1) {
                    if (cmd[0] == "source") {
                        pathReplace = cmd[1];
                        break;
                    } else if (cmd[0] == "source_on") {
                        pathReplaceOn = cmd[1];
                        useToggle = true;
                    } else if (cmd[0] == "source_off") {
                        pathReplaceOff = cmd[1];
                        break;
                    }
                } 
            }
            
            if (!useToggle){
                auto listItem = new tsl::elm::ListItem(optionName, footer);
            
                listItem->setClickListener([command = option.second, keyName = option.first, subPath = this->subPath, usePattern](uint64_t keys) {
                    if (keys & KEY_A) {
                        if (usePattern) {
                            inSubMenu = false;
                            tsl::changeTo<SelectionOverlay>(subPath, keyName, command);
                        } else {
                            // Interpret and execute the command
                            interpretAndExecuteCommand(command);
                        }
                        return true;
                    } else if (keys & KEY_X) {
                        inSubMenu = false; // Set boolean to true when entering a submenu
                        tsl::changeTo<ConfigOverlay>(subPath, keyName);
                        return true;
                    }
                    return false;
                });

                list->addItem(listItem);
            } else {
                auto toggleListItem = new tsl::elm::ToggleListItem(optionName, false, "On", "Off");
                // Set the initial state of the toggle item
                bool toggleStateOn = isFileOrDirectory(preprocessPath(pathReplaceOn));
                
                toggleListItem->setState(toggleStateOn);

                toggleListItem->setStateChangedListener([toggleStateOn, command = option.second, this](bool state) {
                    if (!state) {
                        // Toggle switched to On
                        if (toggleStateOn) {
                            std::vector<std::vector<std::string>> modifiedCommands = getModifyCommands(command, pathReplaceOn, true);
                            interpretAndExecuteCommand(modifiedCommands);
                        } else {
                            // Handle the case where the command should only run in the source_on section
                            // Add your specific code here
                        }
                    } else {
                        // Toggle switched to Off
                        if (!toggleStateOn) {
                            std::vector<std::vector<std::string>> modifiedCommands = getModifyCommands(command, pathReplaceOff, true, false);
                            interpretAndExecuteCommand(modifiedCommands);
                        } else {
                            // Handle the case where the command should only run in the source_off section
                            // Add your specific code here
                        }
                    }
                });

                list->addItem(toggleListItem);
            }

        }

        // Package Info
        PackageHeader packageHeader = getPackageHeaderFromIni(subConfigIniPath);
        list->addItem(new tsl::elm::CategoryHeader("Package Info"));

        constexpr int lineHeight = 20;  // Adjust the line height as needed
        constexpr int xOffset = 120;    // Adjust the horizontal offset as needed
        constexpr int fontSize = 16;    // Adjust the font size as needed
        constexpr int numEntries = 1;   // Adjust the number of entries as needed

        list->addItem(new tsl::elm::CustomDrawer([lineHeight, xOffset, fontSize, packageHeader](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h) {
            renderer->drawString("Creator\nVersion", false, x, y + lineHeight, fontSize, a(tsl::style::color::ColorText));
            renderer->drawString((packageHeader.creator+"\n"+packageHeader.version).c_str(), false, x + xOffset, y + lineHeight, fontSize, a(tsl::style::color::ColorText));
        }), fontSize * numEntries + lineHeight);
        
        rootFrame->setContent(list);
        
        return rootFrame;
    }

    virtual bool handleInput(uint64_t keysDown, uint64_t keysHeld, touchPosition touchInput, JoystickPosition leftJoyStick, JoystickPosition rightJoyStick) override {
        return handleOverlayMenuInput(inSubMenu, keysHeld, KEY_B);
    }
};



// Main menu
class MainMenu : public tsl::Gui {
private:
    std::string directoryPath = "sdmc:/config/ultrahand/";
    std::string configIniPath = directoryPath + "config.ini";
    std::string fullPath;
    //bool inSubMenu = false; // Added boolean to track submenu state
    //bool inTextMenu = false;
public:
    MainMenu() {}

    virtual tsl::elm::Element* createUI() override {
        inMainMenu = true;
        
        auto rootFrame = new tsl::elm::OverlayFrame("Ultrahand", APP_VERSION);
        auto list = new tsl::elm::List();

        // Add a section break with small text to indicate the "Packages" section
        list->addItem(new tsl::elm::CategoryHeader("Packages"));

        // Create the directory if it doesn't exist
        createDirectory(directoryPath);

        // Load options from INI file
        std::vector<std::pair<std::string, std::vector<std::vector<std::string>>>> options = loadOptionsFromIni(configIniPath, true);

        // Load subdirectories
        std::vector<std::string> subdirectories = getSubdirectories(directoryPath);
        std::sort(subdirectories.begin(), subdirectories.end()); // Sort subdirectories alphabetically
        for (const auto& subdirectory : subdirectories) {
            std::string subdirectoryIcon = "";//"\u2605 "; // Use a folder icon (replace with the actual font icon)
            PackageHeader packageHeader = getPackageHeaderFromIni(directoryPath + subdirectory + "/config.ini");
            
            auto listItem = new tsl::elm::ListItem(subdirectoryIcon + subdirectory, packageHeader.version);
            
            listItem->setClickListener([this, subPath = directoryPath + subdirectory](uint64_t keys) {
                if (keys & KEY_A) {
                    //inSubMenu = true; // Set boolean to true when entering a submenu
                    inMainMenu = false;
                    tsl::changeTo<SubMenu>(subPath);
                    
                    
                    return true;
                }
                else if (keys & KEY_X) {
                    //inSubMenu = true; // Set boolean to true when entering a submenu
                    //inTextMenu = true; // Set boolean to true when entering a submenu
                    inMainMenu = false;
                    tsl::changeTo<ConfigOverlay>(subPath);
                    
                    
                    return true;
                }
                return false;
            });

            list->addItem(listItem);
        }

        // Add a section break with small text to indicate the "Packages" section
        list->addItem(new tsl::elm::CategoryHeader("Commands"));

        // Populate the menu with options
        for (const auto& option : options) {
            std::string optionName = option.first;
            std::string optionIcon;

            // Check if it's a subdirectory
            //struct stat entryStat;
            fullPath = directoryPath + optionName;
            //if (stat(fullPath.c_str(), &entryStat) == 0 && S_ISDIR(entryStat.st_mode)) {
            //    optionIcon = "+ "; // Use a folder icon (replace with the actual font icon)
            //} else {
            //    optionIcon = "";
            //    //optionIcon = "\uE001"; // Use a command icon (replace with the actual font icon)
            //}
            optionIcon = "";
            auto listItem = new tsl::elm::ListItem(optionIcon + " " + optionName);

            listItem->setClickListener([this, command = option.second, subPath = optionName](uint64_t keys) {
                if (keys & KEY_A) {
                    // Check if it's a subdirectory
                    struct stat entryStat;
                    std::string newPath = directoryPath + subPath;
                    if (stat(fullPath.c_str(), &entryStat) == 0 && S_ISDIR(entryStat.st_mode)) {
                        inMainMenu = false;
                        tsl::changeTo<SubMenu>(newPath);
                    } else {
                        // Interpret and execute the command
                        interpretAndExecuteCommand(command);
                    }

                    return true;
                }
                return false;
            });

            list->addItem(listItem);
        }


        rootFrame->setContent(list);

        return rootFrame;
    }

    virtual bool handleInput(uint64_t keysDown, uint64_t keysHeld, touchPosition touchInput, JoystickPosition leftJoyStick, JoystickPosition rightJoyStick) override {
        return handleOverlayMenuInput(inMainMenu, keysHeld, KEY_B, 000'000'000);
    }
};


// Overlay
class Overlay : public tsl::Overlay {
public:
    virtual void initServices() override {
        // Initialize services
        //tsl::hlp::doWithSmSession([this]{});
        fsdevMountSdmc();
        splInitialize();
        spsmInitialize();
    }

    //virtual void closeThreads() override {
    //    // CloseThreads();
    //}
    
    virtual void exitServices() override {
        spsmExit();
        splExit();
        fsdevUnmountAll();
    }

    virtual void onShow() override {}    // Called before overlay wants to change from invisible to visible state
    virtual void onHide() override {}    // Called before overlay wants to change from visible to invisible state

    virtual std::unique_ptr<tsl::Gui> loadInitialGui() override {
        return initially<MainMenu>();  // Initial Gui to load. It's possible to pass arguments to its constructor like this
    }
};

int main(int argc, char* argv[]) {
    return tsl::loop<Overlay>(argc, argv);
}

