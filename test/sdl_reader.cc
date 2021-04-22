/*
 MOSS - A server for the Myst Online: Uru Live client/protocol
 Copyright (C) 2008-2011  a'moaca'

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <iconv.h>
#include <sys/stat.h>
#include <getopt.h>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <stdexcept>
#include <list>
#include <vector>

#include "machine_arch.h"
#include "protocol.h"
#include "exceptions.h"
#include "UruString.h"
#include "PlKey.h"
#include "Logger.h"
#include "SDL.h"

void print_info(std::list<SDLDesc*> &sdls, std::list<SDLState*> &state, bool print_index = false, bool verbose = false);
void print_state(SDLState *st, int indent = 0);

int main(int argc, char *argv[]) {
	static struct option options[] = {
			{ "verbose", no_argument, 0, 'v' },
			{ "interactive", no_argument, 0, 'i' },
			{ "load", required_argument, 0, 'l' },
			{ 0, 0, 0, 0 } };
	static const char *usage = "Usage: %s [-i] [-l <saved state file>] <SDL file or directory>[...]\n";
	char c;
	opterr = 0;
	char *saved_state = NULL;
	bool interactive = false;
	bool verbose = false;
	while ((c = getopt_long(argc, argv, "ivl:", options, NULL)) != -1) {
		switch (c) {
		case 'i':
			interactive = true;
			break;
		case 'l':
			saved_state = strdup(optarg);
			break;
		case 'v':
			verbose = true;
			break;
		default:
			fprintf(stderr, usage, argv[0]);
			return 1;
		}
	}
	int i = optind;
	if (i >= argc) {
		fprintf(stderr, usage, argv[0]);
		return 1;
	}

	std::list<SDLDesc*> sdls;
	struct stat s;
	for (; i < argc; i++) {
		int ret = stat(argv[i], &s);
		if (ret < 0) {
			fprintf(stderr, "%s does not exist\n", argv[i]);
			return -1;
		} else if (s.st_mode & S_IFDIR) {
			std::string foo(argv[i]);
			if (SDLDesc::parse_directory(NULL, sdls, foo, true, true)) {
				fprintf(stderr, "Error parsing directory %s\n", argv[i]);
				return -1;
			}
		} else {
			std::ifstream file(argv[i], std::ios_base::in);
			if (file.fail()) {
				fprintf(stderr, "Error opening file %s\n", argv[i]);
				return -1;
			}
			try {
				SDLDesc::parse_file(sdls, file);
			} catch (const parse_error &e) {
				fprintf(stderr, "Parse error, line %d: %s\n", e.lineno(), e.what());
			}
		}
	}

	// if we are to read a saved-state file, do so
	std::list<SDLState*> state;
	if (saved_state) {
		std::ifstream file(saved_state, std::ios_base::in);
		if (file.fail()) {
			fprintf(stderr, "Error opening file %s\n", saved_state);
			return -1;
		}
#ifdef ALCUGS_FORMAT
    file.seekg(4); // skip object count
#endif
		if (!SDLState::load_file(file, state, sdls, NULL)) {
			printf("Error loading SDL from file %s\n", saved_state);
		}
	}

	// check on them
	if (!interactive) {
		print_info(sdls, state, true, verbose);
	} else {
		std::string input;
		std::string cmd;
		while (1) {
			std::getline(std::cin, input);
			if (!std::cin.good()) {
				break;
			}
			std::stringstream in(input);
			std::getline(in, cmd, ' ');
			if (!in.fail()) {
				if (cmd[0] == 'h' || cmd[0] == 'H') {
					printf("\th\t\t\thelp\n"
							"\tp\t\t\tprint info\n"
							"\td <index>\t\tprint state at index\n"
							"\te <index>\t\t\"expand\" state at index\n"
							"\tn <name> [version]\tcreate new default SDL\n"
							"\tl <file>\t\tload saved file\n"
							"\tw <file>\t\twrite saved file\n"
							"\tq\t\t\tquit\n");
				} else if (cmd[0] == 'q') {
					break;
				} else if (cmd[0] == 'p') {
					print_info(sdls, state, true, verbose);
				} else if (cmd[0] == 'd') {
					uint32_t index = 0;
					in >> index;
					if (in.fail()) {
						printf("Index required\n");
					} else if (index >= state.size()) {
						printf("Invalid index");
					} else {
						std::list<SDLState*>::iterator siter;
						uint32_t i = 0;
						for (siter = state.begin(); siter != state.end(); siter++) {
							if (i == index) {
								print_state(*siter);
							}
							i++;
						}
					}
				} else if (cmd[0] == 'e') {
					uint32_t index = 0;
					in >> index;
					if (in.fail()) {
						printf("Index required\n");
					} else if (index >= state.size()) {
						printf("Invalid index");
					} else {
						std::list<SDLState*>::iterator siter;
						uint32_t i = 0;
						for (siter = state.begin(); siter != state.end(); siter++) {
							if (i == index) {
								(*siter)->expand();
								print_state(*siter);
							}
							i++;
						}
					}
				} else if (cmd[0] == 'n') {
					std::string name;
					std::getline(in, name, ' ');
					if (in.fail()) {
						printf("'n' requires a name argument\n");
					} else {
						uint32_t version = 0;
						in >> version;
						SDLDesc *desc = SDLDesc::find_by_name(name.c_str(), sdls, version);
						if (!desc) {
							printf("Could not find SDL named '%s'", name.c_str());
							if (version != 0) {
								printf(" version '%d'\n", version);
							} else {
								printf("\n");
							}
						} else {
							SDLState *s = new SDLState(desc);
							s->expand();
							printf("Created new default '%s' SDL\n", name.c_str());

							bool replaced = false;
							std::list<SDLState*>::iterator siter;
							for (siter = state.begin(); siter != state.end(); siter++) {
								SDLState *other = *siter;
								if (other->get_desc() == desc) {
									printf("Replacing read-in state with new state\n");
									replaced = true;
									state.erase(siter);
									s->key() = other->key();
									delete other;
									break;
								}
							}
							if (!replaced) {
								printf("Warning: using invalid PageID -1 and assuming name "
										"AgeSDLHook!\n");
								s->invent_age_key((uint32_t) -1);
							}
							state.push_front(s);
						}
					}
				} else if (cmd[0] == 'l') {
					std::string name;
					std::getline(in, name, ' ');
					if (in.fail()) {
						printf("'l' requires a filename argument\n");
					} else {
						std::ifstream file(name.c_str(), std::ios_base::in);
						if (file.fail()) {
							printf("Error opening file %s\n", name.c_str());
						} else {
#ifdef ALCUGS_FORMAT
							file.seekg(4); // skip object count
#endif
							if (!SDLState::load_file(file, state, sdls, NULL)) {
								printf("Error loading SDL from file %s\n", name.c_str());
							}
						}
					}
				} else if (cmd[0] == 'w') {
					std::string name;
					std::getline(in, name, ' ');
					if (in.fail()) {
						printf("'w' requires a filename argument\n");
					} else {
						std::ofstream file(name.c_str(), std::ios_base::out);
						if (file.fail()) {
							printf("Error opening file %s\n", name.c_str());
						} else {
#ifdef ALCUGS_FORMAT_OUTPUT
							int zero = 0;
							file.write((char*)&zero, 4); // skip object count
#endif
							if (!SDLState::save_file(file, state)) {
								printf("Error saving SDL to file %s\n", name.c_str());
							}
						}
					}
				} else {
					printf("Unknown command '%c'\n", cmd[0]);
				}
			}
		}
	}

	return 0;
}

void print_info(std::list<SDLDesc*> &sdls, std::list<SDLState*> &state, bool print_index, bool verbose) {
	if (verbose || state.empty()) {
    // printf("print_info:desc sdls.size()=%u\n", sdls.size());
		std::list<SDLDesc*>::iterator iter;
		for (iter = sdls.begin(); iter != sdls.end(); iter++) {
			SDLDesc *desc = *iter;
	    // printf("print_info:desc  iter=0x%x\n", iter);
			printf("SDLDesc %s-v%d  Variables: %u Structs %u\n", desc->name(), desc->version(),
					(uint32_t) desc->vars().size(), (uint32_t) desc->structs().size());
			// XXX print some stuff
			printf("  %s\n", desc->c_str(",\n    ")); // XXX
		}
	}

  // printf("print_info:state state.size()=%u\n", state.size());
	std::list<SDLState*>::iterator siter;
	uint32_t index = 0;
	for (siter = state.begin(); siter != state.end(); siter++) {
		// printf("print_info:state  siter=0x%x\n", siter);
		SDLState *s = *siter;
		if (print_index)
			printf("%d: ", index);
		printf("Saved SDL object %s SDL name %s version %d\n",
				s->key().m_name ? s->key().m_name->c_str() : "",
				s->get_desc()->name(),
				s->get_desc()->version());
		// printf("print_info:pre-print_state\n");
		print_state(s, 1);
		index++;
	}
}

void print_state(SDLState *st, int indent) {
	const SDLDesc *desc = st->get_desc();

	char keybuf[64];
	uint8_t w[indent + 3];
	memset(w, ' ', indent + 2);
	w[indent] = '\0';

  char *str = st->key().c_str(keybuf, sizeof(keybuf));
	printf("%sSDLState %s-v%d %s\n", w, desc->name(), desc->version(), (str? str:""));
	w[indent] = ' ';
	indent += 2;
	w[indent] = '\0';

	const std::vector<SDLState::Variable*> &vars = st->vars();
	const std::vector<SDLState::Struct*> &structs = st->structs();

	if (vars.size() > 0)
	  printf("  Variables:\n");
	std::vector<SDLState::Variable*>::const_iterator vi;
	char buf[16384];
	for (vi = vars.begin(); vi != vars.end(); vi++) {
		if (*vi) {
			SDLState::Variable *v = *vi;
			SDLDesc::Variable *d = desc->vars()[v->m_index];
			char *t;

			printf("%s   %s: ", w, d->c_str());

			if (v->m_count > 1)
				printf("[ ");
      for (uint32_t i = 0; i < v->m_count; i++) {
        if (v->m_flags & SDLState::SameAsDefault) {
          printf("%s ", SDLDesc::Variable::c_str(buf, sizeof(buf), d->m_type, d->m_default));
        } else {
          printf("%s ", SDLDesc::Variable::c_str(buf, sizeof(buf), d->m_type, v->m_value[i]));
        }
      }
			if (v->m_count > 1)
				printf("]");
			if (v->m_flags & SDLState::HasTimeStamp) {
				char ftimebuf[30];
				struct tm *ts = localtime(&v->m_ts.tv_sec);
				strftime(ftimebuf, sizeof(ftimebuf), "%Y-%m-%d %H:%M:%S", ts);
				printf(" [%s %d.%06d]", ftimebuf, v->m_ts.tv_sec, v->m_ts.tv_usec);
			}
			printf("  <%s>\n", (t = SDLState::sdl_flag_c_str_alloc(v->m_flags)));
			if (t)
				free(t);
		}
	}

	if (structs.size() > 0)
	  printf("  Structs:\n");
	std::vector<SDLState::Struct*>::const_iterator si;
	for (si = structs.begin(); si != structs.end(); si++) {
		if (*si) {
			SDLState::Struct *s = *si;
			SDLDesc::Struct *d = desc->structs()[s->m_index];

			printf("%s   %s: ", w, d->m_name);
			if (s->m_flags & SDLState::SameAsDefault) {
				printf("DEFAULT: ");
				// XXX
				printf("\n");
			} else {
				printf("\n");
				for (uint32_t j = 0; j < s->m_count; j++) {
					if (s->m_count > 1) {
						printf("%s [\n", w);
					}
					print_state(&s->m_child[j], indent + 2);
					if (s->m_count > 1) {
						printf("%s ]\n", w);
					}
				}
			}
		}
	}
}
