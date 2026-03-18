---------------------------------------------------------------------------------
 _____ _____ ___  ______  _____ _   _ ___________       _____ _____ _    ______ 
/  ___|_   _/ _ \ | ___ \/  ___| | | |_   _| ___ \     |  __ \  _  | |   |  _  \
\ `--.  | |/ /_\ \| |_/ /\ `--.| |_| | | | | |_/ /_____| |  \/ | | | |   | | | |
 `--. \ | ||  _  ||    /  `--. \  _  | | | |  __/______| | __| | | | |   | | | |
/\__/ / | || | | || |\ \ /\__/ / | | |_| |_| |         | |_\ \ \_/ / |___| |/ / 
\____/  \_/\_| |_/\_| \_|\____/\_| |_/\___/\_|          \____/\___/\_____/___/  
---------------------------------------------------------------------------------                                                                                
Developer: Tianji Gao
Date: 10/15/2025
---------------------------------------------------------------------------------
How to Use:
	Keyboard Support:
		-F1 Draw debug rings and lines
		-F8 Force restart whole game
		-T Slow down whole game (0.1 delta time)
		-O Run one step of the game
		-P Pause whole game
		-I Generate new asteroids
		-SPACEE Generate new bullets or start game at attract mode
		-N Respawn player (only if player is dead)
		-E Accelerate players spaceship (only if player is alive)
		-S Rotate players spaceship counterclockwise (only if player is alive)
		   Select left upgrade at player upgrade board
		-F Rotate players spaceship clockwise (only if player is alive)
		   Select right upgrade at player upgrade board
		-ENTER Confirm upgrade at player upgrade board
		-ESC Switch from game mode to attract mode or shut down application in attract mode
		-K Cheat button for kill all entities in game
	Xbox Controller Support:
		-RIGHT_STICK Control player spaceship orientation and accelerate 
		(0~1 Stick Input -> 0%~100% MAX_ACCELERATION)
		-A Generate new bullets or start game at attract mode
		-START Respawn player (only if player is dead)
		-DEPAD_LEFT Select left upgrade at player upgrade board
		-DEPAD_RIGHT Select right upgrade at player upgrade board
		-X Confirm upgrade at player upgrade board
---------------------------------------------------------------------------------
Known Issues:
	-If there is too many audio being played, the sound system will be ruined.
	-Under unknown conditions, the final wave of enemies has the probability of spawning not according to the configured entity numbers, but only spawning a bunch of wasps.
	-When player stay at their birth point without moving, any damage taken will be multiplied several times and instantly kill the player.
---------------------------------------------------------------------------------
Deep Learning:
	“Keep it simple and stupid!” I believe this is the most important programming philosophy I've learned while developing the Starship project. Looking over the developers I've seen before, most of them were deeply addicted to technological advancements by blindly matching complex algorithms, specific programming language features, fancy design patterns, and frameworks, and I was probably one of them. However, this tends to ignore the fundamental purpose of a game: to be fun. No matter how advanced the technology used, if it fails to deliver as much fun gameplay as possible within a limited time, the game project would not be valuable. It's not a good idea to spend extra effort and time building tools that aren't needed right now just to solve problems that are imagined to be possible in the future. Instead, we should validate ideas faster and then solve problems as they arise. In this project's development, I prioritized listing the fun features I wanted to implement and then directly building them one by one, rather than first considering higher-level tools or frameworks before starting gameplay development. This approach proved effective. After implementing one feature the helper functions intended only for that feature could often be useful for other features by duplicating and/or making few modifications. For example, I initially wanted to create a shockwave effect when enemies were hit. However, I realized that when implementing character shields, I had already implemented a method for drawing gradient discs. For the shockwave, I could directly achieve this by interpolating the radius over time and using the existing method. This significantly improved my development efficiency. Furthermore, the rapid feedback from seeing the results of my work kept me in a positive feedback loop throughout the project's development.