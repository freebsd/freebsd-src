#!/bin/sh

# Basic test for ls_gui
# This is a very basic test and might need a more sophisticated approach
# for thorough GUI testing (e.g., using a GUI testing framework).

LS_GUI_PATH="gui/ls_gui" # Corrected path

if [ ! -f "$LS_GUI_PATH" ]; then
    echo "Error: ls_gui executable not found at $LS_GUI_PATH"
    echo "Please build it first by running 'make' in the gui directory."
    exit 1
fi

echo "Attempting to run ls_gui. This test will pass if the application launches without immediate errors."
echo "Manual verification of GUI behavior is recommended."

# Try to run the GUI application in the background for a short period.
# This is not a functional test of the GUI's logic, but checks if it launches.
if "$LS_GUI_PATH" --version > /dev/null 2>&1; then
    echo "ls_gui launched successfully (checked --version)."
    # Add more sophisticated tests here if a GUI testing framework is available
    # For now, we'll assume if it launches and doesn't crash immediately, it's a basic pass.
    # Example: Try to get a window list and see if our window appears (highly OS/toolkit dependent)
    # For a simple C/GTK app, this is hard to do from a shell script without dedicated tools.
else
    echo "Error: ls_gui did not launch successfully or encountered an error."
    exit 1
fi

echo "Basic GUI launch test passed."
exit 0
