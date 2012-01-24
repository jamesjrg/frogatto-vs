#ifndef EDITOR_HPP_INCLUDED
#define EDITOR_HPP_INCLUDED

#include <boost/function.hpp>
#include <boost/scoped_ptr.hpp>
#include <stack>
#include <vector>

#include "geometry.hpp"
#include "key.hpp"
#include "level.hpp"
#include "level_object.hpp"
#include "stats.hpp"

static const int EDITOR_MENUBAR_HEIGHT = 40;
static const int EDITOR_SIDEBAR_WIDTH = 220;

namespace gui {
class dialog;
}

namespace editor_dialogs {
class character_editor_dialog;
class editor_layers_dialog;
class group_property_editor_dialog;
class property_editor_dialog;
class segment_editor_dialog;
class tileset_editor_dialog;
}

class editor_menu_dialog;
class editor_mode_dialog;

class editor
{
public:
	static void edit(const char* level_cfg, int xpos=-1, int ypos=-1);
	static std::string last_edited_level();

	editor(const char* level_cfg);
	~editor();
	void edit_level();

	void load_stats();
	void show_stats();
	void download_stats();
	const std::vector<stats::record_ptr>& stats() const { return stats_; }

	struct tileset {
		static void init(wml::const_node_ptr node);
		explicit tileset(wml::const_node_ptr node);
		std::string category;
		std::string type;
		int zorder;
		int x_speed;
		int y_speed;
		boost::shared_ptr<tile_map> preview;
		bool sloped;
	};

	struct enemy_type {
		static void init(wml::const_node_ptr node);
		explicit enemy_type(const custom_object_type& type);
		wml::const_node_ptr node;
		std::string category;
		entity_ptr preview_object;
		const frame* preview_frame;
	};

	struct tile_selection {
		bool empty() const { return tiles.empty(); }
		std::vector<point> tiles;
	};

	const tile_selection& selection() const { return tile_selection_; }

	const std::vector<tileset>& all_tilesets() const;
	int get_tileset() const { return cur_tileset_; }
	void set_tileset(int index);

	const std::vector<enemy_type>& all_characters() const;

	int get_object() const { return cur_object_; }
	void set_object(int index);

	enum EDIT_TOOL { TOOL_ADD_RECT, TOOL_SELECT_RECT, TOOL_MAGIC_WAND, TOOL_PENCIL, TOOL_PICKER, TOOL_ADD_OBJECT, TOOL_SELECT_OBJECT, TOOL_EDIT_SEGMENTS, NUM_TOOLS };
	EDIT_TOOL tool() const;
	void change_tool(EDIT_TOOL tool);

	level& get_level() { return *lvl_; }
	const level& get_level() const { return *lvl_; }

	void save_level();
	void save_level_as(const std::string& filename);
	void quit();
	bool confirm_quit();
	void zoom_in();
	void zoom_out();

	void undo_command();
	void redo_command();

	void edit_object_type();
	void new_object_type();

	void close() { done_ = true; }

	void edit_level_properties();

	//make the selected objects part of a group
	void group_selection();

	bool face_right() const { return face_right_; }

	//switch the current facing.
	void toggle_facing();

	void duplicate_selected_objects();

	void run_script(const std::string& id);

	//function which gets the expected layer at which a certain tile id appears.
	int get_tile_zorder(const std::string& tile_id) const;
	void add_tile_rect(int zorder, const std::string& tile_id, int x1, int y1, int x2, int y2);

	enum EXECUTABLE_COMMAND_TYPE { COMMAND_TYPE_DEFAULT, COMMAND_TYPE_DRAG_OBJECT };

	//function to execute a command which will go into the undo/redo list.
	//normally any time the editor mutates the level, it should be done
	//through this function
	void execute_command(boost::function<void()> command, boost::function<void()> undo, EXECUTABLE_COMMAND_TYPE type=COMMAND_TYPE_DEFAULT);

	//functions to begin and end a group of commands. This is used when we
	//are going to execute a bunch of commands, and from the point of view of
	//undoing, they should be viewed as a single operation.
	//When end_command_group() is called, all calls to execute_command since
	//the corresponding call to begin_command_group() will be rolled up
	//into a single command.
	//
	//These functions are re-entrant.
	void begin_command_group();
	void end_command_group();

private:
	void reset_dialog_positions();

	void handle_mouse_button_down(const SDL_MouseButtonEvent& event);
	void handle_mouse_button_up(const SDL_MouseButtonEvent& event);
	void handle_key_press(const SDL_KeyboardEvent& key);
	void handle_scrolling();

	void handle_object_dragging(int mousex, int mousey);
	void handle_drawing_rect(int mousex, int mousey);

	void process_ghost_objects();
	void remove_ghost_objects();
	void draw() const;
	void draw_selection(int xoffset, int yoffset) const;

	void add_tile_rect(int x1, int y1, int x2, int y2);
	void remove_tile_rect(int x1, int y1, int x2, int y2);
	void select_tile_rect(int x1, int y1, int x2, int y2);
	void select_magic_wand(int xpos, int ypos);

	void set_selection(const tile_selection& s);

	void move_object(entity_ptr e, int delta_x, int delta_y);

	bool editing_objects() const { return tool_ == TOOL_ADD_OBJECT || tool_ == TOOL_SELECT_OBJECT; }
	bool editing_tiles() const { return !editing_objects(); }

	//functions which add and remove an object from a level, as well as
	//sending the object appropriate events.
	void add_multi_object_to_level(entity_ptr e);
	void add_object_to_level(entity_ptr e);
	void remove_object_from_level(entity_ptr e);

	void mutate_object_value(entity_ptr e, const std::string& value, variant new_value);

	CKey key_;

	boost::intrusive_ptr<level> lvl_;
	int zoom_;
	int xpos_, ypos_;
	int anchorx_, anchory_;

	//if we are dragging an entity around, this marks the position from
	//which the entity started the drag.
	int selected_entity_startx_, selected_entity_starty_;
	std::string filename_;

	EDIT_TOOL tool_;
	bool done_;
	bool face_right_;
	int cur_tileset_;

	int cur_object_;

	tile_selection tile_selection_;

	boost::scoped_ptr<editor_menu_dialog> editor_menu_dialog_;
	boost::scoped_ptr<editor_mode_dialog> editor_mode_dialog_;
	boost::scoped_ptr<editor_dialogs::character_editor_dialog> character_dialog_;
	boost::scoped_ptr<editor_dialogs::editor_layers_dialog> layers_dialog_;
	boost::scoped_ptr<editor_dialogs::group_property_editor_dialog> group_property_dialog_;
	boost::scoped_ptr<editor_dialogs::property_editor_dialog> property_dialog_;
	boost::scoped_ptr<editor_dialogs::tileset_editor_dialog> tileset_dialog_;

	boost::scoped_ptr<editor_dialogs::segment_editor_dialog> segment_dialog_;

	gui::dialog* current_dialog_;

	//if the mouse is currently down, drawing a rect.
	bool drawing_rect_, dragging_;

	struct executable_command {
		boost::function<void()> redo_command;
		boost::function<void()> undo_command;
		EXECUTABLE_COMMAND_TYPE type;
	};

	std::vector<executable_command> undo_, redo_;

	//a temporary undo which is used for when we execute commands on
	//a temporary basis -- e.g. for a preview -- so we can later undo them.
	boost::scoped_ptr<executable_command> tmp_undo_;

	//indexes into undo_ which records the beginning of the current 'group'
	//of commands. When begin_command_group() is called, a value is added
	//set to the size of undo_. When end_command_group() is called, all
	//commands with index > the top value are aggregated into a single command,
	//and the top value is popped.
	std::stack<int> undo_commands_groups_;

	std::vector<entity_ptr> ghost_objects_;

	std::vector<stats::record_ptr> stats_;

	int level_changed_;
	int selected_segment_;
};

#endif
