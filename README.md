# Ungrat3ful

To - Do:

- Tile-Based map
	- Hexagonal
  	- Overhead birds-eye view camera
- "Dice" based randomization
- Psychadelic based magic system
- Drugs as healing items
- MKULTRA and Project Montauk
- Possible locations for committing ecoterrorism:
	- Alaska
 	- Amazon Rainforest
  	- Previously undiscovered landmass?
  	- Antarctica
  	- Great Barrier Reef
  	- The Everglades

- Mission ideas:
	- Infiltrate area, destroy construction equipment
	- Infiltrate area, kill target

   
- Graphics:
	- Parallax 2D pixel art
  	- Over the shoulder 'Pokemon' style ui for selecting enemies to use an attack on

Seasons!
- Pumpkin Spicecore character customizations

- Peasant tide

Unibomber Manifesto as religious artifact

Class system:

Archer/crossbow class:
Can ignore enemy cover and obstacles 
Good movement points

Heavy:
Has more explosives
Low movement points
Explodes if killed

Assault:
Shotgun and sword
Most movement points

Hacker:
Can hack robotic enemies
Low health and average movement

Medic:
Can heal adjacent units
medic used anything but bandages to heal units. All healing drugs cause debuffs to the healed unit.
High health and average movement

######some g++ linking commands:

works on arch virtual machine:
g++ main.cpp -I/usr/include/SDL2/ -lSDL2 -lSDL2_image -lSDL2_ttf
work on arch WSL (but arch wsl cannot run the executable binary):
g++ main.cpp $(pkg-config --cflags --libs sdl2) -lSDL_image -lSDL_ttf
