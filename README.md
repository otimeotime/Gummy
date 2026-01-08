# Gummy
A Gunny-like game for the Network Programming course.

## Techstack:
1. Core: C++, SDL2
2. Database: PostgreSQL version 18.1

## Program Files Structure
```
gummy/
├── assets/
    ├── maps/
    ├── sprites/
    ├── font.ttf # Font of the program
├── build/obj/... # Pre-compile object files
├── src/
    ├── common/
            ├── gui/
                ├── Vector2D.hpp # Coordinate presentation of UI component
            ├── network/
                ├── Packet.hpp # Define network package 
                ├── PacketStructs.hpp # Define payload of each type
                ├── PacketType.hpp # Define payload type
                ├── TCPSocketUtils.cpp # Define utility function for a socket
                ├── TCPSocket.hpp # Define TCP socket
    ├── client/
        ├── core/
            ├── Game.cpp # Program's frontend wrapper
            ├── GameState.hpp # Abstract screen class
            ├── InputHandler.cpp # Handle user input
            ├── StateMachine.cpp # Manage program states (screens)
            ├── TextureManager.cpp # Draw graphic on screen
            ├── Window.cpp # Define the screen with SDL2
        ├── network/
            ├── ClientSocket.cpp # Define socket for client
        ├── scenes/
            ├── SceneDashboard.cpp # Dashboard (homepage) screen
            ├── SceneGame.cpp # Ingame screen
            ├── SceneLogin # Login screen
            ├── SceneRegister # Register screen
            ├── SceneViewProfile # View profile screen
            ├── TerminalScene # Endgame screen
        ├── ui/
            ├── Button # Button component 
            ├── Text # Text component
            ├── TextInput # Textfield component
    ├── ingame_server/
        ├── core/
            ├── GameServer.cpp # Ingame server
        ├── database/
            ├── MatchRecorder.cpp # Endgame process (update database and player information)
        ├── logic/
            ├── GameRoom.hpp # Ingame manager
            ├── MapLoader.hpp # Draw map on the screen
            ├── PhysicsEngine.hpp # Ingame physics mechanism
            ├── Player.hpp # Define player ingame
        ├── replay/   
            ├── ReplayFile.hpp # Define replay file
    ├── service_server/
        ├── core/
            ├── ServiceServer.cpp # Service server (at the dashboard)
        ├── database/
            ├── DatabaseServer.cpp # Database connection
            ├── UserDAO.cpp # Database operation on User
        ├── logic/
            ├── AuthServer.cpp # Database operation on authentication
```   
