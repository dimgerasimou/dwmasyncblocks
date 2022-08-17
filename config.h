#ifndef CONFIGURATION
#define CONFIGURATION

#define CLICKABLE_BLOCKS 1
#define LEADING_DELIMITER 0
#define DELIMITER " | "

#define SCRIPTPATH(SCRIPTNAME) "$HOME/.local/bin/statusbar/" #SCRIPTNAME

const Block blocks[] = {
	/*Command                  Update Interval   Update Signal */
	{ SCRIPTPATH("volume"),    0,                10 },
	{ SCRIPTPATH("memory"),    6,                0  },
	{ SCRIPTPATH("internet"),  5,                1  },
	{ SCRIPTPATH("battery"),   5,                2  },
	{ SCRIPTPATH("keyboard"),  0,                3  },
	{ SCRIPTPATH("kernel"),    360,              4  },
	{ SCRIPTPATH("clock"),     30,               5  },
};

#endif /* ifndef CONFIGURATION */
