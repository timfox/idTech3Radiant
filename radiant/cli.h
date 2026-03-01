#pragma once

extern bool g_headless;

bool cli_is_headless( int argc, char* argv[] );
int cli_main( int argc, char* argv[] );
