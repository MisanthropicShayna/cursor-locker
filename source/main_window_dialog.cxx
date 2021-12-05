#include "main_window_dialog.hxx"
#include "ui_main_window_dialog.h"

enum struct ACTIVATION_METHOD {
    NOTHING         =   0b00000000,
    HOTKEY          =   0b00000001,
    PROCESS_IMAGE   =   0b00000010,
    WINDOW_TITLE    =   0b00000100,
};

void MainWindow::insertActivationParameterWidget(QWidget* widget, bool enable, bool unhide) {
    if(widget != nullptr) {
        if(widget->layout() != ui->hlActivationParameter) {
            ui->hlActivationParameter->insertWidget(0, widget, 0);
        }

        widget->setEnabled(enable);
        widget->setHidden(!unhide);
    }
}

void MainWindow::removeActivationParameterWidget(QWidget* widget, bool disable, bool hide) {
    if(widget != nullptr) {
        if(widget->layout() == ui->hlActivationParameter) {
            ui->hlActivationParameter->removeWidget(widget);
        }

        widget->setEnabled(!disable);
        widget->setHidden(hide);
    }
}

bool MainWindow::registerAmpHotkey() {
    QDebugConsoleContext { dbgConsole, "registerAmpHotkey" };

    BOOL result { RegisterHotKey(HWND(winId()), ampHotkeyId, MOD_NOREPEAT | ampHotkeyModifiersBitmask, ampHotkeyVkid) };

    dbgConsole->log({"RegisterHotKey(HWND(winId), 0x",
                     QString::number(ampHotkeyId, 16), \
                     ", MOD_NOREPEAT | ",
                     QString::number(ampHotkeyModifiersBitmask),
                     ", 0x",
                     QString::number(ampHotkeyVkid, 16),
                     ") returned ",
                     result ? "true" : "false"
                 });

    return result;
}

bool MainWindow::unregisterAmpHotkey() {
    QDebugConsoleContext { dbgConsole, "unregisterAmpHotkey" };

    BOOL result { UnregisterHotKey(HWND(winId()), ampHotkeyId) };

    if(result) {
        dbgConsole->log({"UnregisterHotKey(HWND(winId), 0x", QString::number(ampHotkeyId, 16), ") returned true"});
    }

    return result;
}

QString MainWindow::setAmpHotkeyVkid(const quint32& vkid) {
    ampHotkeyVkid = vkid;

    unregisterAmpHotkey();

    if(selectedActivationMethod == ACTIVATION_METHOD::HOTKEY) {
        if(vkid) {
            registerAmpHotkey();

            const QString& vkid_hexstr  {
                QString { "0x%1" }.arg(vkid, 2, 16, QLatin1Char { '0' })
            };

            ui->linActivationParameter->setText(vkid_hexstr);

            return vkid_hexstr;
        } else {
            ui->linActivationParameter->clear();
        }
    }

    return QString {};
}

QString MainWindow::setAmpHotkeyVkid(const QString& vkid_hexstr) {
    QDebugConsoleContext dbg_console_context { dbgConsole, "setAmpHotkeyVkid(QString)" };

    if(!vkid_hexstr.size()) {
        return setAmpHotkeyVkid(0x00);
    }

    char hex_conversion_buffer[5];

    ZeroMemory(hex_conversion_buffer,
               sizeof(hex_conversion_buffer));

    bool conversion_success { false };
    qint32 vkid = vkid_hexstr.toInt(&conversion_success, 16);

    if(conversion_success) {
        return setAmpHotkeyVkid(static_cast<uint8_t>(vkid));
    } else {
        dbgConsole->log({"Failed to convert hexadecimal VKID string into an integer \"",
                        vkid_hexstr, "\""
                        }, QDebugConsole::LL_ERROR);

        return QString {};    // Return empty QString on conversion failure.
    }
}

void MainWindow::setAmToHotkey() {
    QDebugConsoleContext dbg_console_context { dbgConsole, "setActivationMethodToHotkey" };

    dbgConsole->log("Set activation method to: hotkey.");
    dbgConsole->log("VKID List for hotkeys: http://www.kbdedit.com/manual/low_level_vk_list.html");

    selectedActivationMethod = ACTIVATION_METHOD::HOTKEY;

    unregisterAmpHotkey();

    connect(this, &MainWindow::targetHotkeyWasPressed,
            this, &MainWindow::activateBecauseTargetHotkeyWasPressed);

    ui->linActivationParameter->setPlaceholderText("VKID, e.g. 0x6A (Numpad *)");

    ampHotkeyModifiersBitmask = ampwHotkeyModifierDropdown->GetModifierCheckStateAsBitmask();

    if(ampHotkeyVkid) {
        const QString& vkid_hexstr  {
            QString { "0x%1" }.arg(ampHotkeyVkid, 2, 16, QChar { '0' })
        };

        ui->linActivationParameter->setText(vkid_hexstr);

        registerAmpHotkey();
    }

    insertActivationParameterWidget(ampwHotkeyRecorder, false);
    insertActivationParameterWidget(ampwHotkeyModifierDropdown, false);
}

void MainWindow::unsetAmToHotkey() {
    unregisterAmpHotkey();

    disconnect(this, &MainWindow::targetHotkeyWasPressed,
               this, &MainWindow::activateBecauseTargetHotkeyWasPressed);

    removeActivationParameterWidget(ampwHotkeyModifierDropdown);
    removeActivationParameterWidget(ampwHotkeyRecorder);
}

void MainWindow::setAmpProcessImageName(const QString& process_image_name) {
    amParamProcessImageName = process_image_name;

    if(selectedActivationMethod == ACTIVATION_METHOD::PROCESS_IMAGE) {
        ui->linActivationParameter->setText(amParamProcessImageName);
    }
}

void MainWindow::setAmToProcessImageName() {
    QDebugConsoleContext dbg_console_context { dbgConsole, "setAmToProcessImageName" };

    selectedActivationMethod = ACTIVATION_METHOD::PROCESS_IMAGE;
    ui->linActivationParameter->setPlaceholderText("Image name, e.g. TESV.exe, SkyrimSE.exe, etc");

    if(amParamProcessImageName.size()) {
        ui->linActivationParameter->setText(amParamProcessImageName);
        dbgConsole->log({"Loaded amParamProcessImageName \"", amParamProcessImageName, "\""});
    }

    timedActivationMethodConnection = connect(timedActivationMethodTimer,   SIGNAL(timeout()),
                                              this,                         SLOT(activateIfTargetProcessRunning()));

    insertActivationParameterWidget(btnSpawnProcessScanner, false);
    btnSpawnProcessScannerConnection = connect(btnSpawnProcessScanner, &QPushButton::clicked,
                                               std::bind(&MainWindow::spawnProcessScannerDialog, this, ProcessScanner::PROCESS_MODE));

    dbgConsole->log("Activation method has been set to process image name.");
}

void MainWindow::unsetAmToProcessImageName() {
    QDebugConsoleContext dbg_console_context { dbgConsole, "unsetAmToProcessImageName" };

    disconnect(timedActivationMethodConnection);
    removeActivationParameterWidget(btnSpawnProcessScanner);
    disconnect(btnSpawnProcessScannerConnection);
}

void MainWindow::setAmpForegroundWindowTitle(const QString& foreground_window_title) {
    amParamForegroundWindowTitle = foreground_window_title;

    if(selectedActivationMethod == ACTIVATION_METHOD::WINDOW_TITLE) {
        ui->linActivationParameter->setText(amParamForegroundWindowTitle);
    }
}

void MainWindow::setAmToForegroundWindowTitle() {
    QDebugConsoleContext dbg_console_context { dbgConsole, "setAmToForegroundWindowTitle" };

    dbgConsole->log("Activation mode set to window title. Right click the edit button to grab the name of the next foreground window that you select.");

    selectedActivationMethod = ACTIVATION_METHOD::WINDOW_TITLE;
    ui->linActivationParameter->setPlaceholderText("Window title, e.g. Skyrim, Skyrim Special Edition");

    if(amParamForegroundWindowTitle.size()) {
        ui->linActivationParameter->setText(amParamForegroundWindowTitle);
        dbgConsole->log({"Set activation parameter text field to: ", amParamForegroundWindowTitle});
    }

    timedActivationMethodConnection = connect(timedActivationMethodTimer,    SIGNAL(timeout()),
                                              this,                          SLOT(activateIfForegroundWindowMatchesTarget()));

    insertActivationParameterWidget(btnStartWindowGrabber, false);
    insertActivationParameterWidget(btnSpawnProcessScanner, false);
    btnSpawnProcessScannerConnection = connect(btnSpawnProcessScanner, &QPushButton::clicked,
                                               std::bind(&MainWindow::spawnProcessScannerDialog, this, ProcessScanner::WINDOW_MODE));

}

void MainWindow::unsetAmToForegroundWindowTitle() {
    disconnect(timedActivationMethodConnection);
    removeActivationParameterWidget(btnSpawnProcessScanner);
    removeActivationParameterWidget(btnStartWindowGrabber);
    disconnect(btnSpawnProcessScannerConnection);
}

bool MainWindow::loadStylesheetFile(const std::string& file_path) {
    std::ifstream input_stream { file_path, std::ios::binary };

    if(input_stream.good()) {
        std::string style_sheet { std::istreambuf_iterator<char>(input_stream), std::istreambuf_iterator<char>() };
        input_stream.close();

        dbgConsole->log({"Read ", QString::number(style_sheet.size()), " bytes from stylesheet @ ", QString::fromStdString(file_path)});

        setStyleSheet(QString::fromStdString(style_sheet));
        return true;
    } else {
        return false;
    }
}

void MainWindow::beepBoop(QList<QPair<int, int>> freqdur_list) {
    if(!muteBeepBoop) {
        for(QPair<int,int>& pair : freqdur_list) {
            Beep(pair.first, pair.second);
        }
    }
}

RECT MainWindow::getForegroundWindowRect() {
    HWND foreground_window_handle { GetForegroundWindow() };
    RECT foreground_window_rect;

    GetWindowRect(foreground_window_handle, &foreground_window_rect);

    return foreground_window_rect;
}

void MainWindow::enableCursorLock() {
    RECT foreground_window_rect { getForegroundWindowRect() };
    ClipCursor(&foreground_window_rect);
}

void MainWindow::disableCursorLock() {
    ClipCursor(nullptr);
}

bool MainWindow::toggleCursorLock() {
    static bool toggle { false };

    if(toggle ^= true) {
        enableCursorLock();
    } else {
        disableCursorLock();
    }

    return toggle;
}

bool MainWindow::nativeEvent(const QByteArray& event_type, void* message, qintptr* result) {
    Q_UNUSED(event_type);
    Q_UNUSED(result);

    MSG* msg { reinterpret_cast<MSG*>(message) };

    /* msg->lParam Is a 64-bit integer and stores the VKID of the pressed hotkey in byte 3/8 (little-endian)
     * and so it cannot be directly compared with ampHotkeyVkid, as it stores the VKID in byte 1/1 so the
     * comparison will always fail. Instead, lParam should be bitshifted 16 bits to the right so that the VKID
     * is located in byte 1/8, and then bitwise AND with 0xFF should be performed to exclude any other bits that
     * may be present in the 64-bit integer, and leave only the bits in position 1/8 which should be the VKID.
     * After this is done, it may be compared as normal without issue.
     *
     *                                                  lParam >> 16
     * ????     ????     ????     ????     ????     |--VKID-----------v
     * 00000000 00000000 00000000 00000000 00000000 01101010 00000000 00000000
     * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

    if (msg->message == WM_HOTKEY) {
        if(msg->wParam == ampHotkeyId && ((msg->lParam >> 16) & 0xFF) == ampHotkeyVkid) {
            emit targetHotkeyWasPressed();
            return true;
        }

        dbgConsole->log({"nativeEvent got MSG WM_HOTKEY, wParam=0x",
                      QString::number(msg->wParam, 16),
                      ", lParam=0x",
                      QString::number(msg->lParam, 16)
                     });
    }

    return QMainWindow::nativeEvent(event_type, message, result);
}

// ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MainWindow's SLOTS below this line!!!
// ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MainWindow::activateBecauseTargetHotkeyWasPressed() {
    if(toggleCursorLock()) {
        dbgConsole->log("Activated cursor lock via hotkey.");
        beepBoop({{500,20}, {700, 20}});
    } else {
        dbgConsole->log("Deactivated cursor lock via hotkey.");
        beepBoop({{700,20}, {500, 20}});
    }
}

void MainWindow::activateIfTargetProcessRunning() {
    if(!amParamProcessImageName.size()) {
        disableCursorLock();
        return;
    }

    HANDLE process_snapshot { CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,  0) };

    if(process_snapshot == INVALID_HANDLE_VALUE) {
        dbgConsole->log("Invalid handle value for snapshot of type TH32CS_SNAPPROCESS. Cannot see running tasks!", QDebugConsole::QDebugConsole::LL_ERROR);
        return;
    }

    PROCESSENTRY32 process_entry_32;
    process_entry_32.dwSize = sizeof(PROCESSENTRY32);

    if(Process32First(process_snapshot, &process_entry_32)) {
        static bool first_find { true };
        bool process_found { false };

        do {
            if(amParamProcessImageName == process_entry_32.szExeFile) {
                process_found = true;
                break;
            }
        } while(Process32Next(process_snapshot, &process_entry_32));

        if(process_found) {
            enableCursorLock();

            if(first_find) {
                dbgConsole->log({"Enabling lock because target process was found: ", amParamProcessImageName});
                first_find = false;

                beepBoop({{500,20}, {700,20}});
            }
        } else {
            disableCursorLock();

            if(!first_find) {
                dbgConsole->log({"Disabling lock because target process was lost: ", amParamProcessImageName});
                first_find = true;

                beepBoop({{700,20}, {500,20}});
            }
        }
    }

    CloseHandle(process_snapshot);
}

void MainWindow::activateIfForegroundWindowMatchesTarget() {
    if(!amParamForegroundWindowTitle.size()) {
        disableCursorLock();
        return;
    }

    static bool first_find { true };

    char window_title_buffer[256];
    std::fill(window_title_buffer, window_title_buffer + sizeof(window_title_buffer), 0x00);

    HWND foreground_window { GetForegroundWindow() };
    GetWindowTextA(foreground_window, window_title_buffer, sizeof(window_title_buffer));

    if(amParamForegroundWindowTitle == window_title_buffer) {
        enableCursorLock();

        if(first_find) {
            dbgConsole->log({"Enabling lock because target window was found: ", amParamForegroundWindowTitle});
            first_find = false;

            beepBoop({{500,20}, {700,20}});
        }
    } else {
        disableCursorLock();

        if(!first_find) {
            dbgConsole->log({"Disabling lock because target window was lost: ", amParamForegroundWindowTitle});
            first_find = true;

            beepBoop({{700,20}, {500,20}});
        }
    }
}

void MainWindow::handleModifierListBitmaskChanged(const quint32& bitmask) {
    ampHotkeyModifiersBitmask = bitmask;
}

void MainWindow::changeActivationMethod(int method_index) {
    /* Clear linActivationParameter's text to allow the placeholder text of
     * the selected activation method to be displayed later on.
     * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
    ui->linActivationParameter->clear();
    timedActivationMethodTimer->stop();

    disableCursorLock(); // Ensure the cursor lock is disabled before switching over to a new activation method.

    unsetAmToHotkey();
    unsetAmToProcessImageName();
    unsetAmToForegroundWindowTitle();

    static const QList<quint32>& timed_activation_methods_indexes { 2, 3 };

    switch(method_index) {
    case 0 :
        dbgConsole->log("Activation method set to nothing.");

        selectedActivationMethod = ACTIVATION_METHOD::NOTHING;
        ui->linActivationParameter->setPlaceholderText("No activation method selected");
        break;

    case 1 :
        setAmToHotkey();
        break;

    case 2 :
        setAmToProcessImageName();
        break;

    case 3 :
        setAmToForegroundWindowTitle();
        break;
    }

    if(timed_activation_methods_indexes.contains(method_index)) {
        timedActivationMethodTimer->start(500);
    }
}

bool MainWindow::changeActivationMethod(const QString& method) {
    if(JsonSettingsDialog::ActivationMethodResolverSTOI.contains(method)) {
        ui->cbxActivationMethod->setCurrentIndex(JsonSettingsDialog::ActivationMethodResolverSTOI[method]);
        return true;
    }

    return false;
}

void MainWindow::updateUiWithRecordedWindowsHotkey(QHotkeyInput::WindowsHotkey windows_hotkey) {
    QDebugConsoleContext dbg_console_context { dbgConsole, "updateUiWithRecordedWindowsHotkey" };

    ui->linActivationParameter->setText(QString::number(windows_hotkey.Vkid, 16));
    ampwHotkeyModifierDropdown->SetModifierCheckStateFromBitmask(windows_hotkey.Modifiers);

    dbgConsole->log({"Ui has been set to use recorded hotkey: ", windows_hotkey.ToString()});
}

void MainWindow::updateHotkeyInputWithNewModifierBitmask(const quint32& new_modifier_bitmask) {
    ampwHotkeyRecorder->clear();
}

void MainWindow::editActivationMethodParameter() {
    if(ui->btnEditActivationParameter->text() == "Edit" && selectedActivationMethod != ACTIVATION_METHOD::NOTHING) {
        ui->btnEditActivationParameter->setText("Confirm");
        ui->linActivationParameter->setEnabled(true);
        ui->cbxActivationMethod->setEnabled(false);

        btnSpawnProcessScanner->setEnabled(true);
        btnStartWindowGrabber->setEnabled(true);

        if(selectedActivationMethod == ACTIVATION_METHOD::HOTKEY) {
            ampwHotkeyModifierDropdown->setEnabled(true);
            ampwHotkeyRecorder->setEnabled(true);
            ampwHotkeyRecorder->StartRecording();
        }
    } else if(ui->btnEditActivationParameter->text() == "Confirm") {
        ui->btnEditActivationParameter->setText("Edit");
        ui->linActivationParameter->setEnabled(false);
        ui->cbxActivationMethod->setEnabled(true);

        btnSpawnProcessScanner->setEnabled(false);
        btnStartWindowGrabber->setEnabled(false);

        ampwHotkeyModifierDropdown->setEnabled(false);
        ampwHotkeyRecorder->setEnabled(false);
        ampwHotkeyRecorder->StopRecording();

        switch(selectedActivationMethod) {
        case ACTIVATION_METHOD::HOTKEY : {
            ampHotkeyModifiersBitmask = ampwHotkeyModifierDropdown->GetModifierCheckStateAsBitmask();
            setAmpHotkeyVkid(ui->linActivationParameter->text());
            ampwHotkeyRecorder->clear();
            break;
        }

        case ACTIVATION_METHOD::PROCESS_IMAGE :
            setAmpProcessImageName(ui->linActivationParameter->text());
            break;

        case ACTIVATION_METHOD::WINDOW_TITLE :
            setAmpForegroundWindowTitle(ui->linActivationParameter->text());
            break;

        case ACTIVATION_METHOD::NOTHING :
            break;
        }
    }
}

void MainWindow::foregroundWindowGrabberTimerTimeout() {
    const auto& stop_and_reset {
        [&]() -> void {
            btnStartWindowGrabber->setText("Grab");

            windowGrabberTimer->stop();
            windowGrabberTimerTimeoutCounter = 0;

            ui->linActivationParameter->clear();
            ui->linActivationParameter->setEnabled(true);
            ui->btnEditActivationParameter->setEnabled(true);
            btnSpawnProcessScanner->setEnabled(true);
        }};

    if(++windowGrabberTimerTimeoutCounter > windowGrabberTimerMaxTimeouts) {
        stop_and_reset();
    } else {
        beepBoop({{200, 20}});

        ui->linActivationParameter->setText(
                    "Switch to target window within timeframe: "
                    + QString::number(windowGrabberTimerTimeoutCounter)
                    + " / "
                    + QString::number(windowGrabberTimerMaxTimeouts)
                    );

        HWND foreground_window { GetForegroundWindow() };

        // Other window has been selected, as foreground_window doesn't match this program's window.
        if(foreground_window != HWND(winId())) {
            beepBoop({{900, 20}, {900, 20}});
            stop_and_reset();

            char window_title[256];
            ZeroMemory(window_title, sizeof(window_title));
            qint32 bytes_written { GetWindowText(foreground_window, window_title, sizeof(window_title)) };
            dbgConsole->log({"GetWindowText() wrote ", QString::number(bytes_written), " bytes to char window_title[256]"});

            ui->linActivationParameter->setText(window_title);
        }
    }
}

void MainWindow::onWindowGrabberButtonClicked() {
    if(btnStartWindowGrabber->text() == "Grab") {
        btnStartWindowGrabber->setText("Stop");

        ui->linActivationParameter->setEnabled(false);
        ui->btnEditActivationParameter->setEnabled(false);
        btnSpawnProcessScanner->setEnabled(false);

        windowGrabberTimer->start(500);
    } else if(btnStartWindowGrabber->text() == "Stop") {
        btnStartWindowGrabber->setText("Grab");

        windowGrabberTimer->stop();
        windowGrabberTimerTimeoutCounter = 0;

        ui->linActivationParameter->clear();
        ui->linActivationParameter->setEnabled(true);
        ui->btnEditActivationParameter->setEnabled(true);
        btnSpawnProcessScanner->setEnabled(true);
    }
}

void MainWindow::spawnProcessScannerDialog(ProcessScanner::SCAN_SCOPE process_scanner_scope) {
    QDebugConsoleContext { dbgConsole, "spawnProcessScannerDialog" };

    if(processScannerDialog == nullptr) {
        dbgConsole->log("Constructing new ProcessScannerDialog instance.");

        processScannerDialog = new ProcessScannerDialog(process_scanner_scope, this);

        ui->btnEditActivationParameter->setEnabled(false);
        btnStartWindowGrabber->setEnabled(false);

        connect(processScannerDialog, &ProcessScannerDialog::destroyed, [&](QObject*) -> void {
            ui->btnEditActivationParameter->setEnabled(true);
            btnStartWindowGrabber->setEnabled(true);
            processScannerDialog = nullptr;
        });

        connect(processScannerDialog, &ProcessScannerDialog::treeSelectionMade, [&](const QString& selection) -> void {
            ui->linActivationParameter->setText(selection);
            delete processScannerDialog;
            processScannerDialog = nullptr;
        });
    }

    processScannerDialog->show();
}

void MainWindow::spawnSettingsDialog() {
    if(jsonSettingsDialog == nullptr) {
        jsonSettingsDialog = new JsonSettingsDialog { jsonConfigFilePath, this };

        connect(jsonSettingsDialog, &JsonSettingsDialog::destroyed, [&]() -> void {
            jsonSettingsDialog = nullptr;
        });

        jsonSettingsDialog->show();
    }
}

void MainWindow::setMuteBeepBoopState(bool state) {
    muteBeepBoop = state;
    ui->btnMuteBeepBoop->setText(state ? "Unmute" : "Mute");
}

void MainWindow::toggleMuteBeepBoop() {
    setMuteBeepBoopState(muteBeepBoop ^ true);
}

MainWindow::MainWindow(QWidget* parent)
    :
      // Debug Console & Related Widgets Initialization

      QMainWindow                         { parent                     },
      ui                                  { new Ui::MainWindow         },

      dbgConsole                          { new QDebugConsole { this } },
      vblDebugConsoleLayout               { new QVBoxLayout            },
      dbgConsoleContextMenu               { new QMenu { dbgConsole }   },

      jsonConfigFilePath                  { "./defaults.json"          },

      selectedActivationMethod            { ACTIVATION_METHOD::NOTHING },

      // Hotkey activation method member variables initialization
      ampwHotkeyModifierDropdown          { new QKbModifierList {this} },
      ampwHotkeyRecorder                  { new QHotkeyInput  { this } },
      ampHotkeyModifiersBitmask           { WINMOD_NULLMOD             },
      ampHotkeyVkid                       { 0x000                      },
      ampHotkeyId                         { 0x1A4                      },

      // Process scanner member variables initialization.
      processScannerDialog                { nullptr                    },     // ProcessScannerDialog instance, must be nullptr as spawnProcessScannerDialog takes care of construction and destruction.
      btnSpawnProcessScanner              { new QPushButton   { this } },     // QPushButton connected to spawnProcessScannerDialog further down in the constructor.
      amParamProcessImageName             { QString { "" }             },
      amParamForegroundWindowTitle        { QString { "" }             },

      // Foreground window grabber
      windowGrabberTimerMaxTimeouts       { 15   },
      windowGrabberTimerTimeoutCounter    { NULL },
      windowGrabberTimer                  { new QTimer        { this } },
      btnStartWindowGrabber               { new QPushButton   { this } },

      timedActivationMethodTimer          { new QTimer        { this } },
      jsonSettingsDialog                  { nullptr                    },

      muteBeepBoop                        { false                      }

{
    ui->setupUi(this);

    // Configure Main Window Flags & Set Initial Window Size
    // --------------------------------------------------
    setWindowFlags(
                Qt::Dialog
                | Qt::CustomizeWindowHint
                | Qt::WindowTitleHint
                | Qt::WindowCloseButtonHint
                | Qt::WindowMinimizeButtonHint
                | Qt::WindowMaximizeButtonHint
                );

    HWND desktop_window_handle { GetDesktopWindow() };
    RECT desktop_window_rect;

    GetWindowRect(desktop_window_handle, &desktop_window_rect);
    CloseHandle(desktop_window_handle);

    resize(23 * desktop_window_rect.right / 100, 16 * desktop_window_rect.bottom / 100);

    // Debug console layout configuration.
    vblDebugConsoleLayout->addWidget(dbgConsole);
    ui->grpDebugConsole->setLayout(vblDebugConsoleLayout);

    // Set ampwHotkeyModifierDropdown initial values.
    ampwHotkeyModifierDropdown->setEnabled(false);
    ampwHotkeyModifierDropdown->setHidden(true);
    ampwHotkeyModifierDropdown->setMinimumWidth(90);
    ampwHotkeyModifierDropdown->addItem("Modifiers");
    ampwHotkeyModifierDropdown->AddItemsFromBitmask(WINMOD_ALT | WINMOD_CONTROL | WINMOD_SHIFT | WINMOD_WIN);

    // Set ampwHotkeyRecorder initial values.
    ampwHotkeyRecorder->setHidden(true);
    ampwHotkeyRecorder->setPlaceholderText("Hotkey Recorder");

    // btnSpawnProcessScanner initial values.
    btnSpawnProcessScanner->setText("Select");
    btnSpawnProcessScanner->setEnabled(false);
    btnSpawnProcessScanner->setHidden(true);
    btnSpawnProcessScanner->setMinimumHeight(25);

    btnStartWindowGrabber->setText("Grab");
    btnStartWindowGrabber->setEnabled(false);
    btnStartWindowGrabber->setHidden(true);
    btnStartWindowGrabber->setMinimumHeight(25);

    // Event Connections
    // ====================================================================================================
    // Static QML UI File Widgets --------------------------------------------------
    connect(ui->cbxActivationMethod,           SIGNAL(currentIndexChanged(int)),
            this,                              SLOT(changeActivationMethod(int)));

    connect(ui->btnEditActivationParameter,    SIGNAL(clicked()),
            this,                              SLOT(editActivationMethodParameter()));

    connect(ui->btnMuteBeepBoop,               SIGNAL(clicked()),
            this,                              SLOT(toggleMuteBeepBoop()));

    connect(ampwHotkeyRecorder,                SIGNAL(WindowsHotkeyRecorded(QHotkeyInput::WindowsHotkey)),
            this,                              SLOT(updateUiWithRecordedWindowsHotkey(QHotkeyInput::WindowsHotkey)));

    connect(ampwHotkeyModifierDropdown,        SIGNAL(ModifierBitmaskChanged(const quint32&)),
            this,                              SLOT(updateHotkeyInputWithNewModifierBitmask(const quint32&)));

    connect(windowGrabberTimer,                SIGNAL(timeout()),
            this,                              SLOT(foregroundWindowGrabberTimerTimeout()));

    connect(btnStartWindowGrabber,             SIGNAL(clicked()),
            this,                              SLOT(onWindowGrabberButtonClicked()));

    connect(ui->btnSettings,                   SIGNAL(clicked()),
            this,                              SLOT(spawnSettingsDialog()));

    QFileInfo json_file_info { jsonConfigFilePath };

    if(json_file_info.exists() && json_file_info.isFile()) {
        const JsonSettingsDialog::JsonSettings& json_settings { JsonSettingsDialog::LoadSettingsFromJsonFile(this, jsonConfigFilePath) };

        setAmpForegroundWindowTitle(json_settings.ForegroundWindowTitle);
        setAmpProcessImageName(json_settings.ProcessImageName);
        setAmpHotkeyVkid(json_settings.HotkeyVkid);
        setMuteBeepBoopState(json_settings.InitialMuteState);

        ampwHotkeyModifierDropdown->SetModifierCheckStateFromBitmask(json_settings.HotkeyModifierBitmask);

        changeActivationMethod(json_settings.ActivationMethod);
    }

    if(loadStylesheetFile("./style_sheet.qss")) {
        dbgConsole->log("Loaded QSS stylesheet ./style_sheet.qss - theme has been applied.");
    } else {
        dbgConsole->log("Cannot open style_sheet.qss, using default Windows style.", QDebugConsole::LL_WARNING);
    }
}

MainWindow::~MainWindow() {
    disableCursorLock();
    delete ui;
}
