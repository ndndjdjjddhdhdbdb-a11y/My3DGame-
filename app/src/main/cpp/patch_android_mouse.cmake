# Apply a small, idempotent patch to the Raylib Android backend.
set(RAYLIB_ANDROID_INPUT "${CMAKE_SOURCE_DIR}/deps/raylib/src/platforms/rcore_android.c")
if(NOT EXISTS "${RAYLIB_ANDROID_INPUT}")
    message(FATAL_ERROR "Raylib Android input source not found: ${RAYLIB_ANDROID_INPUT}")
endif()

file(READ "${RAYLIB_ANDROID_INPUT}" RAYLIB_ANDROID_SOURCE)
string(FIND "${RAYLIB_ANDROID_SOURCE}" "BOLT_SURVIVAL_ANDROID_MOUSE_INPUT" BOLT_MOUSE_PATCHED)
if(BOLT_MOUSE_PATCHED EQUAL -1)
    set(BOLT_MOUSE_MARKER "    if (type == AINPUT_EVENT_TYPE_MOTION)\n    {")
    set(BOLT_MOUSE_HANDLER [=[
    // BOLT_SURVIVAL_ANDROID_MOUSE_INPUT
    // Android's stock Raylib backend treats a Bluetooth mouse as touch input.
    // Handle pointer hover/move and physical mouse buttons separately.
    if ((type == AINPUT_EVENT_TYPE_MOTION) &&
        ((source & AINPUT_SOURCE_MOUSE) == AINPUT_SOURCE_MOUSE))
    {
        int32_t mouseAction = AMotionEvent_getAction(event);
        unsigned int mouseFlags = mouseAction & AMOTION_EVENT_ACTION_MASK;
        int32_t mousePointer = (mouseAction & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >>
                               AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        float mouseX = AMotionEvent_getX(event, mousePointer);
        float mouseY = AMotionEvent_getY(event, mousePointer);
        float mouseWidthRatio = (float)(CORE.Window.screen.width + CORE.Window.renderOffset.x)/
                                (float)CORE.Window.display.width;
        float mouseHeightRatio = (float)(CORE.Window.screen.height + CORE.Window.renderOffset.y)/
                                 (float)CORE.Window.display.height;

        CORE.Input.Mouse.previousPosition = CORE.Input.Mouse.currentPosition;
        CORE.Input.Mouse.currentPosition = (Vector2){
            mouseX*mouseWidthRatio - (float)CORE.Window.renderOffset.x/2,
            mouseY*mouseHeightRatio - (float)CORE.Window.renderOffset.y/2
        };

        int32_t mouseButtons = AMotionEvent_getButtonState(event);
        CORE.Input.Mouse.currentButtonState[MOUSE_BUTTON_LEFT] =
            (mouseButtons & AMOTION_EVENT_BUTTON_PRIMARY) ? 1 : 0;
        CORE.Input.Mouse.currentButtonState[MOUSE_BUTTON_RIGHT] =
            (mouseButtons & AMOTION_EVENT_BUTTON_SECONDARY) ? 1 : 0;
        CORE.Input.Mouse.currentButtonState[MOUSE_BUTTON_MIDDLE] =
            (mouseButtons & AMOTION_EVENT_BUTTON_TERTIARY) ? 1 : 0;

        if (mouseFlags == AMOTION_EVENT_ACTION_SCROLL)
        {
            CORE.Input.Mouse.currentWheelMove.x +=
                AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_HSCROLL, mousePointer);
            CORE.Input.Mouse.currentWheelMove.y +=
                AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_VSCROLL, mousePointer);
        }

        return 1;
    }

]=])
    string(FIND "${RAYLIB_ANDROID_SOURCE}" "${BOLT_MOUSE_MARKER}" BOLT_MOUSE_MARKER_POS)
    if(BOLT_MOUSE_MARKER_POS EQUAL -1)
        message(FATAL_ERROR "Raylib Android input source has an unexpected layout")
    endif()
    string(REPLACE "${BOLT_MOUSE_MARKER}" "${BOLT_MOUSE_HANDLER}\n${BOLT_MOUSE_MARKER}"
                   RAYLIB_ANDROID_SOURCE "${RAYLIB_ANDROID_SOURCE}")
    string(FIND "${RAYLIB_ANDROID_SOURCE}" "BOLT_SURVIVAL_ANDROID_MOUSE_INPUT" BOLT_MOUSE_VERIFY)
    if(BOLT_MOUSE_VERIFY EQUAL -1)
        message(FATAL_ERROR "Failed to patch Raylib Android mouse input")
    endif()
    file(WRITE "${RAYLIB_ANDROID_INPUT}" "${RAYLIB_ANDROID_SOURCE}")
endif()
