/*
   AI Assistant - Editor-side AI integration for Radiant
   Structured context extraction and placement plan execution.
*/

#pragma once

class QMainWindow;

void AIAssistant_createDock( QMainWindow* window );
void AIAssistant_open();
void AIAssistant_toggleShown();
void AIAssistant_destroy();
void AIAssistant_registerPreferencesPage();

bool AIAssistant_enabled();
