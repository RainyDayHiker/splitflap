This folder contains my "final" STLs for my SplitFlap build.

Inspiration, lots of ideas, and lots of measurements from https://www.printables.com/model/1043629-compact-splitflap-display.

Flaps are generated with Instruction and NotoEmoji fonts.  Two sets - one for most of the characters and one with weather icons for a single character.  Biggest change from original set is that I replaced some colors with emojis.

Major changes from original design:
- Spool: modeled so that it can be printed without support and assembled without glue
- Most hardware changed to M3 nuts and bolts (all except motor mounting)
- All nuts are "captured" and not loose
- Frame pieces now have wire channels to ensure wires are kept out of the way of the flaps
- Frame extended to provide mounting for panels to enclose the entire display

Print settings:
- Flaps:
	- .10mm layer height, .10mm first layer height (a multi-material setup is your friend)
	- 1 perimeter
	- Satin plate (best balance of getting the surface close to the top and looking good)
	- Print flaps alternating tops so that each letter is either a top or satin finish and not mixed
	- Increased purge for white filament to make sure it is white after printing black
	- I used PLA since I found a good PLA white and black to give good contrast
- Everything else:
	- .20mm SPEED or STRUCTURAL
	- Set "Bridging Angle" to 45 for Frame prints (not required but improves the wire channels since they have overhangs and I kept finding my slicer wanted to do long runs for at least one of them)
	- No supports needed - there are overhangs, but they'll print well enough on a good printer
	- Orientation of parts should be correct
	- Whatever build plate gives the surface you want on the Side and Top panels
	- Rubber Foot in TPU

Part list:
- Prints:
	- For each Flap unit:
		- Set of flaps
		- Frame - Left
		- Frame - Right
		- Spool - Left
		- Spool - Right
		- Panel - Bottom
	- For each row of flaps:
		- Panel - Left
		- Panel - Right
	- For every 3 columns:
		- Back Panel: first one should be the Buddy one the rest should be the regular one
		- Top Panel: First two sets of three columns are left and right, center for the rest.  
			- Note that I didn't model a top that is sized for a 3 column display - can be added at some point although there is also no way to mount the driver correctly in this case either.  Mostly things are modeled for multiples of 6 columns.
	- For every 6 flaps:
		- Chainlink Driver Mount Left
		- Chainlink Driver Mount Right
	- One Chainlink Buddy Mount for each display
	- Rubber Foot (Optional): as many as make sense
	
- Hardware:
	- Note: 
		- The black hardware is a design choice - normal hardware works as well
		- Button head bolts are recommended but other head types would likely work for all cases other than the motor and sensor mounts where a low clearance is desired/required
	- Per motor:
		- M4 hex nut (x2)
		- M4x8 button head (x2)
	- For each Flap unit:
		- M3x10 button head (x4) (connect flaps together)
		- M3x8 button head (x1) (connect sensor)
		- M3 hex nut (x5)
		- NOTE: Reduce this count by the amount of hardware for the right and left panels
	- For each Flap unit that is connected vertically to a flap below it:
		- M3x10 button head (x2) -- TODO: check!
		- M3 hex nut (x6)
	- For each Panel - Right:
		- M3 black hex nut (x4)
		- M3x8 black (x4)
	- For each Panel - Left:
		- M3x10 black (x4)
		- M3 hex nut (x4)
		- M3x6 black (x1) (for sensor) (Note that this is tight but works - an M3x8 would be fine as well)
		- M3 black hex nut (x1) (for sensor)
	- For each Panel - Top (of any type):
		- M3x8 black (x6) -- TODO: check!
		- M3 hex nut (x6)
	- For each Panel - Back (of any type):
		- M3x16 button head (x3)
		- M3 hex nut (x3)
	- For each Chainlink Buddy Mount:
		- M3x8 button head (x4) (Board to Mount)
		- M3x10 button head (x1) (Mount to Frame) CHECK!!
		- M3 hex nut (x5)
	- For each Chainlink Driver Mount set:
		- M3x8 button head (x4) (Board to Mounts)
		- M3x8 button head (x2) (Mount to Frame)
		- M3 hex nut (x6)
	- For each rubber foot:
 		- M3x8 button head (x1)
		- M3 hex nut (x1)
		- NOTE: if this foot shares a mounting location with a Chainlink Mount, no additional hardware needed but the Mount to Frame bolt needs to be changed to an M3x12 button head

Assembly instructions:
- General notes:
	- Check and clean up any loose filament in wire channels in Frame pieces
	- For the inserted nuts (motor mount and connections between flaps):
		- Clean out the channels since there are printed overhangs that can leave threads
		- After inserting the nut, use a hex wrench to ensure it is centered and, ideally, partially insert a bolt to make sure things are perfectly aligned before assembly
	- For the press fit nuts:
		- Either use a long bolt inserted through the hole to pull the nut into place
		- Or use a short bolt and tighten the nut into place (techniques work better in different spots)
		- Nylon nuts don't need this
	- Highly recommend hex wrenches with ball ends so that they can be used at an angle
	- All rights/lefts are when looking at the front of the display
	- Test everything as you go along - although it is possible to split the display in the middle to get at a specific module, it isn't fun

- Preperation before assembly:
	- Motors: 
		- Remove blue cover
		- Tape down the wires with electrical tape
		- Repeat for all motors
	
	- Spool:
		- Two halves should slide together and notch in with the tabs
		- Add a flap and, if needed, slightly twist the spool to make sure the flap is not twisted
			- If you put the spool on a flat surface with the flap extending out perpendicularly, the flap and the surface should be at right angles
			- A small bit of glue can be used to lock things in place but I have not found it necessary
		- Add the rest of the flaps
		- Repeat per unit being built
	
	- All Chainlink Mounts:
		- Add 2 or 4 M3 hex nuts to each of the mounts
	
	- Rubber Foot:
		- Add an M3 hex nut to each foot
		
	- Frame - Right:
		- Insert 2 M4 hex nuts into the motor mount
		- Insert 4 M3 hex nuts into each of the frame cross pieces where the next unit will attach
		- Insert 3 M3 hex nuts into the top cross pieces for mounting the top and back panels
		- Insert 1 M3 hex nut into the bottom rear cross piece if this unit will have a mount for a Chainlink Driver or Buddy and will not have a foot (if there is a foot, it will have the nut instead of the frame - see note later for my recommendations here)
		- Insert 1 M3 hex nut below motor mount for sensor attachment, if this is not the rightmost unit
		- Mount motor with 2 M4x8 button head bolts

- Putting it together
	- First row:
		1. Start from lower right unit
		1. Attach a Panel - Right to a Frame - Right with four black nuts and M3x8 bolts (black is a fashion choice)
			- Ensure motor cable is correctly captured by the wire channel and not pinched
		1. Optional: Add feet and Chainlink Mounts - these can be added after assembly but are most easily added now - especially the front feet.
		1. Insert Spool on to the motor
		1. Add a Panel - Bottom to Frame - Right (this can be done later given how flexible the panel is but easier to do it now)
		1. Attach a Frame - Left to the next Frame - Right with an M3x8 bolt through the sensor PCB and into the Frame - Right
			- Ensure motor cable routes through the wire channel and isn't pinched
			- Ensure sensor cable routes through the wire channel and isn't pinched
			- Also recommend a small bit of electrical tape covering the solder pins of the connector (just insurance that the spool won't catch on the pins)
		1. Attach the new frame assembly to the display via 4 M3x10 bolts through the four frame connectors
		1. Repeat from step 3 until at the last unit of the width desired then continue
		1. Attach the last Frame - Left to the Panel - Left with an M3x6 black bolt into a black nut similar to the above steps attaching a Frame - Left to a Frame - Right
			- Ensure sensor cable routes through the wire channel and isn't pinched
		1. Add a spool on to the motor
		1. Attach the Left assembly to the display with 4 M3x10 black bolts through the four frame connectors.

	- Additional rows are the same steps as the first row except:
		- Skip installing the bottom panel
		- Attach the upper frame to the lower frame with 2 M3x10 (TODO CHECK!) bolts
			- For any upper unit that will have a Chainlink mount installed, easiest to do it now and use an M3x12 bolt instead although it can be done at the end as well
	
	- Finally:
		1. Install any moounts and feet not yet installed
		1. Install Chainlink hardware
		1. Install back panels with M3x16 bolts, routing power cables through the hole in the Back - Buddy
		1. Install top panels using M3x8 black bolts - using the appropriate top panel for left/center/right sets of three modules

- For a 6 unit wide setup, here is my recommendation for hardware mounts and feet from right to left (from front):
	- Foot
	- Driver
	- Foot
	- Empty
	- Driver
	- Foot + Buddy

- For a 12 unit wide setup, here is my recommendation for hardware mounts and feet from right to left (from front):
	- Foot
	- Driver
	- Foot
	- Empty
	- Foot + Driver
	- Empty
	- Empty
	- Foot + Driver
	- Empty
	- Foot
	- Driver
	- Foot + Buddy
