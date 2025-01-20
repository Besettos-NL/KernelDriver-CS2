#pragma once
#pragma once

// menu
bool show_menu = true;

// box types
static const int BoxTypes[] = { 1, 2, 3 };
static const char* BoxTypesNames[] = { "Normal", "cornered", "Filled" };

int esp_BoxType = 0;

// esp
bool toggle_esp = false;
bool toggle_box_esp = false;
bool toggle_healthbar_esp = false;
bool toggle_healthtext_esp = false;
bool toggle_snaplines_esp = false;
float box_esp_color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
float snaplines_esp_color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };


bool toggle_aimbot = false;
