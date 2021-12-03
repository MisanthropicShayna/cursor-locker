#include "hotkey_input_widget.hpp"

bool QHotkeyInput::WindowsHotkey::operator==(const WindowsHotkey& other) const {
    return    QtModifiers      == other.QtModifiers
           && QtModifierKeys   == other.QtModifierKeys
           && QtKey            == other.QtKey
           && Modifiers        == other.Modifiers
           && ScanCode         == other.ScanCode
           && Vkid             == other.Vkid;
}

bool QHotkeyInput::WindowsHotkey::operator!=(const WindowsHotkey& other) const {
    return !(*this==other);
}

QHotkeyInput::WindowsHotkey::operator QString() const {
    QString modifier_sequence { WinApiKbModifierBitmaskToQString(Modifiers, false) };

    if(Vkid) {
        modifier_sequence += QKeySequence { QtKey }.toString();
    }

    return modifier_sequence;
}

QHotkeyInput::WindowsHotkey::operator bool() const {
    return    QtModifiers              != NULL
           || QtModifierKeys.size()    != NULL
           || QtKey                    != NULL
           || ScanCode                 != NULL
           || Vkid                     != NULL;
}

QString QHotkeyInput::WindowsHotkey::ToString() const {
    return QString(*this);
}

void QHotkeyInput::WindowsHotkey::Clear() {
    QtModifierKeys.clear();

    QtModifiers = Qt::KeyboardModifier::NoModifier;
    QtKey = static_cast<Qt::Key>(NULL);

    Modifiers    = NULL;
    ScanCode     = NULL;
    Vkid         = NULL;
}

bool QHotkeyInput::event(QEvent* event) {
    const auto& event_type { event->type() };

    if(event_type != QEvent::KeyPress && event_type != QEvent::KeyRelease) {
        return QLineEdit::event(event);
    }

    if(!isRecording) return false;

    QKeyEvent*                key_event          { reinterpret_cast<QKeyEvent*>(event)    };
    const Qt::Key&            qtkey              { static_cast<Qt::Key>(key_event->key()) };
    const WINAPI_MODIFIER&    winapi_modifier    { QtKeyModifierToWinApiKbModifier(qtkey) };

    if(event_type == QEvent::KeyRelease) {
        if(windowsHotkey.Vkid && !winapi_modifier && windowsHotkey != lastWindowsHotkeyEmitted) {
            emit WindowsHotkeyRecorded(windowsHotkey);
            lastWindowsHotkeyEmitted = windowsHotkey;
            mainKeyEstablished = true;
        } else if(!windowsHotkey.Vkid && winapi_modifier) {
            windowsHotkey.Modifiers &= ~winapi_modifier;

            if(!windowsHotkey.Modifiers) {
                windowsHotkey.Clear();
            }

            setText(windowsHotkey.ToString());
        }
    }

    else if(event_type == QEvent::KeyPress && !key_event->isAutoRepeat() && key_event->key()) {
        if(mainKeyEstablished && winapi_modifier) {
            windowsHotkey.Clear();
            mainKeyEstablished = false;
        }

        if(winapi_modifier) {
            windowsHotkey.Modifiers |= winapi_modifier;
            windowsHotkey.QtModifierKeys.append(qtkey);
        } else {
            windowsHotkey.Vkid        = key_event->nativeVirtualKey();
            windowsHotkey.ScanCode    = key_event->nativeScanCode();
            windowsHotkey.QtKey       = qtkey;
        }

        setText(windowsHotkey.ToString());
    }

    event->setAccepted(true);
    return true;
}

void QHotkeyInput::StartRecording() {
    if(!isRecording) {
        isRecording = true;
    }
}

void QHotkeyInput::StopRecording() {
    if(isRecording) {
        isRecording = false;
        windowsHotkey.Clear();
    }
}

bool QHotkeyInput::IsRecording() const {
    return isRecording;
}

QHotkeyInput::QHotkeyInput(QWidget* parent)
    :
      QLineEdit             { parent      },
      mainKeyEstablished    { false       },
      isRecording           { false       }
{
    windowsHotkey.Clear();
}
