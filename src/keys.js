.pragma library

// The numpad without NumLock sends navigation keys. A calculator has no use
// for those, and a compositor can hand a freshly focused window a keymap
// that drops NumLock, so read the numpad as digits either way.
function numpadDigit(event) {
    if (!(event.modifiers & Qt.KeypadModifier))
        return "";
    switch (event.key) {
    case Qt.Key_Insert: return "0";
    case Qt.Key_End: return "1";
    case Qt.Key_Down: return "2";
    case Qt.Key_PageDown: return "3";
    case Qt.Key_Left: return "4";
    case Qt.Key_Clear: return "5";
    case Qt.Key_Right: return "6";
    case Qt.Key_Home: return "7";
    case Qt.Key_Up: return "8";
    case Qt.Key_PageUp: return "9";
    case Qt.Key_Delete: return ".";
    }
    return "";
}

function hasCommandModifier(event) {
    return (event.modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier)) !== 0;
}
