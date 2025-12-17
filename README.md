# Gummy
A Gunny-like game for the Network Programming course.

## Techstack:
1. Core: C++, SQL2
2. Database: PostgreSQL version 18.1

## Program Files Structure
src/
├── common/                 
│   ├── network/
│   │   ├── Packet.h       
│   │   └── PacketUtils.cpp  
│   ├── model/
│   │   ├── GameConfig.h    
│   │   └── UserInfo.h       
│   └── utils/               
│
├── client/                
│   ├── main.cpp            
│   ├── core/              
│   │   ├── Window.h
│   │   ├── TextureManager.h
│   │   └── InputHandler.h
│   ├── ui/                  
│   │   ├── Button.h
│   │   ├── TextInput.h
│   │   └── ProgressBar.h
│   ├── scenes/              
│   │   ├── SceneManager.h   
│   │   ├── SceneLogin.h
│   │   ├── SceneLobby.h
│   │   └── SceneGame.h
│   └── network/
│       └── ClientSocket.h   
│
├── server_service/         
│   ├── main.cpp
│   ├── core/
│   │   └── ServiceCore.h    
│   ├── database/
│   │   ├── DatabaseManager.h 
│   │   └── UserDAO.h       
│   └── logic/
│       ├── AuthManager.h   
│       └── MatchMaker.h     
│
└── server_ingame/        
    ├── main.cpp
    ├── core/
    │   └── GameServerCore.h
    ├── logic/
    │   ├── GameRoom.h      
    │   ├── Player.h         
    │   └── PhysicsEngine.h  
    └── map/
        └── MapLoader.h     
