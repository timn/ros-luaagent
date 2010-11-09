
/***************************************************************************
 *  luaagent.cpp - Lua Agent main application
 *
 *  Created: Thu Sep  2 15:00:08 2010
 *  Copyright  2010  Tim Niemueller [www.niemueller.de]
 *             2010  Carnegie Mellon University
 *             2010  Intel Labs Pittsburgh
 ****************************************************************************/

/*  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version. A runtime exception applies to
 *  this software (see LICENSE.GPL_WRE file mentioned below for details).
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  Read the full text in the LICENSE.GPL_WRE file in the doc directory.
 */

#include <ros/ros.h>

#include <lua_utils/context.h>
#include <lua_utils/context_watcher.h>

static int lua_add_watchfile(lua_State *L);

class LuaAgentMain : public fawkes::LuaContextWatcher
{
  friend int lua_add_watchfile(lua_State *L);
 public:
  LuaAgentMain(ros::NodeHandle &n)
    : __lua(/* watch files */ false, /* tracebacks */ true),
      __n(n)
  {
    __lua.add_watcher(this);
  }

  // for LuaContextWatcher
  void
  lua_init(fawkes::LuaContext *context) {}

  void
  lua_finalize(fawkes::LuaContext *context)
  {
    __lua.get_global("roslua");		// roslua
    __lua.get_field(-1, "finalize");	// roslua roslua.finalize
    try {
      __lua.pcall();			// roslua
    } catch (Exception &e) {
      printf("Finalize failed: %s", e.what());
    }
    __lua.pop(1);			// ---
  }

  void
  lua_restarted(fawkes::LuaContext *context)
  {
    // Restarted will _not_ be called on the very first initialization, because
    // the watcher is not yet registered. For each reload, we need to run the
    // start script after contexts have been swaped.
    __lua.do_file(LUADIR"/luaagent/ros/start.lua");
  }

  void
  init_lua()
  {
    std::string agent = "herb_agents.IOH2010";
    if (__n.hasParam("/luaagent/agent")) {
      __n.getParam("/luaagent/agent", agent);
    }

    __lua.set_cfunction("add_watchfile", lua_add_watchfile);
    __lua.add_package_dir(LUADIR);
    __lua.add_package("roslua");

    __lua.set_string("AGENT", agent.c_str());
    __lua.set_string("ROS_MASTER_URI", ros::master::getURI().c_str());

    // Cannot do this for proper reloading, calling this manually
    //__lua.set_start_script(LUADIR"/luaagent/ros/start.lua");
    __lua.do_file(LUADIR"/luaagent/ros/start.lua");
  }

  int run()
  {
    init_lua();

    ros::Rate rate(25);
    bool quit = false;
    // run until luaagent stopped
    while (! quit && __n.ok() ) {
      __lua.get_global("roslua");	// roslua
      try {
	// Spin!
	__lua.get_field(-1, "spin");	// roslua roslua.spin
	try {
	  __lua.pcall();		// roslua
	} catch (Exception &e) {
	  printf("%s", e.what());
	}

	// get quite flag
	__lua.get_field(-1, "quit");	// roslua roslua.quit
	quit = __lua.to_boolean(-1);
	__lua.pop(2);			// ---

	__lua.process_fam_events();
      } catch (Exception &e) {
	printf("%s\n", e.what());
      }
      rate.sleep();
    }
    return 0;
  }

 private:
  fawkes::LuaContext __lua;
  ros::NodeHandle &__n;
};

static LuaAgentMain *luaagent;

int
lua_add_watchfile(lua_State *L)
{
  const char *s = luaL_checkstring(L, 1);
  if (s == NULL) luaL_error(L, "Directory argument missing");
  try {
    luaagent->__lua.add_watchfile(s);
  } catch (Exception &e) {
    luaL_error(L, "Adding watch directory failed: %s", e.what());
  }
  return 0;
}

int
main(int argc, char **argv)
{
  ros::init(argc, argv, "luaagentmain");
  ros::NodeHandle n;

  luaagent = new LuaAgentMain(n);
  int rv = luaagent->run();
  delete luaagent;
  return rv;
}
