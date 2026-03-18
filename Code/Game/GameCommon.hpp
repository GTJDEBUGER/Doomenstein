#pragma once
//Debug related setting
constexpr int   DEBUG_DRAWRING_SUBDIVISION        = 32;
constexpr float DEBUG_SLOWDOWN_TIMESCALE          = 0.1f;
//Screen camera related setting				      
constexpr float SCREEN_SIZE_X                     = 1600.f;
constexpr float SCREEN_SIZE_Y                     = 800.f;
constexpr float SCREEN_CENTER_X                   = SCREEN_SIZE_X / 2.f;
constexpr float SCREEN_CENTER_Y                   = SCREEN_SIZE_Y / 2.f;
constexpr float SCREEN_ASPECT                     = SCREEN_SIZE_X / SCREEN_SIZE_Y;
//Camera shake related setting				      
constexpr float CAMERA_SHAKE_MAX_AMP              = 10.f;
constexpr float CAMERA_SHAKE_DECAYSPEED           = 5.f;
//Player related							      
constexpr float PLAYER_MOVE_MAX_SPEED             = 5.f;
constexpr float PLAYER_RUN_MAX_SPEED              = 20.f;
constexpr float PLAYER_VIEW_YAW_SPEED             = 15.f;
constexpr float PLAYER_VIEW_PITCH_SPEED           = 15.f;
constexpr float PLAYER_VIEW_CONTROLLER_MULTIPLIER = 3.f;
constexpr float PLAYER_VIEW_ROLL_SPEED            = 90.f;
struct Vertex;
struct Vec2;
struct Rgba8;
