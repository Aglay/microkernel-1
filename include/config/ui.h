#ifndef __UI_H__
#define __UI_H__

#define UI_WIDTH						80
#define UI_CLI_HISTORY_HEIGHT 			5	//rows
#define UI_CLI_HISTORY_TOP_ROW 			18

// Computed

#define UI_CLI_HISTORY_BOTTOM_ROW 		( 17 + UI_CLI_HISTORY_HEIGHT )
#define UI_CLI_CMD_LINE_ROW				( 18 + UI_CLI_HISTORY_HEIGHT )
#define UI_CLI_CMD_LINE_WIDTH			( UI_WIDTH - 9 )

#endif
