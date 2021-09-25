// This file is part of SmallBASIC
//
// SmallBASIC - External library support (plugins)
//
// This program is distributed under the terms of the GPL v2.0 or later
// Download the GNU Public License (GPL) from www.gnu.org
//
// Copyright(C) 2001 Nicholas Christopoulos

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "common/smbas.h"

#if defined(__MINGW32__)
 #include <windows.h>
 #include <error.h>
 #define WIN_EXTLIB
 #define LIB_EXT ".dll"
#elif defined(_UnixOS)
 #include <dlfcn.h>
 #define LNX_EXTLIB
 #define LIB_EXT ".so"
#endif

#if defined(LNX_EXTLIB) || defined(WIN_EXTLIB)
#include "common/plugins.h"
#include "common/pproc.h"
#include <dirent.h>

#define MAX_SLIBS 64
#define MAX_PARAM 16
#define TABLE_GROW_SIZE 16
#define NAME_SIZE 256
#define PATH_SIZE 1024

typedef int (*sblib_exec_fn)(int, int, slib_par_t *, var_t *);
typedef int (*sblib_getname_fn) (int, char *);
typedef int (*sblib_count_fn) (void);
typedef int (*sblib_init_fn) (const char *);
typedef void (*sblib_close_fn) (void);
typedef const char *(*sblib_get_module_name_fn) (void);

typedef struct {
  char _fullname[PATH_SIZE];
  char _name[NAME_SIZE];
  void *_handle;
  sblib_exec_fn _sblib_proc_exec;
  sblib_exec_fn _sblib_func_exec;
  ext_func_node_t *_func_list;
  ext_proc_node_t *_proc_list;
  uint32_t _id;
  uint32_t _flags;
  uint32_t _proc_count;
  uint32_t _func_count;
  uint32_t _proc_list_size;
  uint32_t _func_list_size;
  uint8_t  _imported;
} slib_t;

static slib_t *plugins[MAX_SLIBS];

#if defined(LNX_EXTLIB)
int slib_llopen(slib_t *lib) {
  lib->_handle = dlopen(lib->_fullname, RTLD_NOW);
  if (lib->_handle == NULL) {
    sc_raise("LIB: error on loading %s\n%s", lib->_name, dlerror());
  }
  return (lib->_handle != NULL);
}

void *slib_getoptptr(slib_t *lib, const char *name) {
  return dlsym(lib->_handle, name);
}

static int slib_llclose(slib_t *lib) {
  if (!lib->_handle) {
    return 0;
  }
  dlclose(lib->_handle);
  lib->_handle = NULL;
  return 1;
}

#elif defined(WIN_EXTLIB)
static int slib_llopen(slib_t *lib) {
  lib->_handle = LoadLibraryA(lib->_fullname);
  if (lib->_handle == NULL) {
    int error = GetLastError();
    switch (error) {
    case ERROR_MOD_NOT_FOUND:
      sc_raise("LIB: DLL dependency error [%d] loading %s [%s]\n", error, lib->_fullname, lib->_name);
      break;
    case ERROR_DYNLINK_FROM_INVALID_RING:
      sc_raise("LIB: DLL build error [%d] loading %s [%s]\n", error, lib->_fullname, lib->_name);
      break;
    default:
      sc_raise("LIB: error [%d] loading %s [%s]\n", error, lib->_fullname, lib->_name);
      break;
    }
  }
  return (lib->_handle != NULL);
}

static void *slib_getoptptr(slib_t *lib, const char *name) {
  return GetProcAddress((HMODULE) lib->_handle, name);
}

static int slib_llclose(slib_t *lib) {
  if (!lib->_handle) {
    return 0;
  }
  FreeLibrary(lib->_handle);
  lib->_handle = NULL;
  return 1;
}
#endif

//
// returns slib_t* for the given id
//
static slib_t *get_lib(int lib_id) {
  if (lib_id < 0 || lib_id >= MAX_SLIBS) {
    return NULL;
  }
  return plugins[lib_id];
}

//
// add an external procedure to the list
//
static int slib_add_external_proc(const char *proc_name, int lib_id) {
  slib_t *lib = get_lib(lib_id);

  if (lib->_proc_list == NULL) {
    lib->_proc_list_size = TABLE_GROW_SIZE;
    lib->_proc_list = (ext_proc_node_t *)malloc(sizeof(ext_proc_node_t) * lib->_proc_list_size);
  } else if (lib->_proc_list_size <= (lib->_proc_count + 1)) {
    lib->_proc_list_size += TABLE_GROW_SIZE;
    lib->_proc_list = (ext_proc_node_t *)realloc(lib->_proc_list, sizeof(ext_proc_node_t) * lib->_proc_list_size);
  }

  lib->_proc_list[lib->_proc_count].lib_id = lib_id;
  lib->_proc_list[lib->_proc_count].symbol_index = 0;
  strlcpy(lib->_proc_list[lib->_proc_count].name, proc_name, sizeof(lib->_proc_list[lib->_proc_count].name));
  strupper(lib->_proc_list[lib->_proc_count].name);

  if (opt_verbose) {
    log_printf("LIB: %d, Idx: %d, PROC '%s'\n", lib_id, lib->_proc_count,
               lib->_proc_list[lib->_proc_count].name);
  }
  lib->_proc_count++;
  return lib->_proc_count - 1;
}

//
// Add an external function to the list
//
static int slib_add_external_func(const char *func_name, uint32_t lib_id) {
  slib_t *lib = get_lib(lib_id);

  if (lib->_func_list == NULL) {
    lib->_func_list_size = TABLE_GROW_SIZE;
    lib->_func_list = (ext_func_node_t *)malloc(sizeof(ext_func_node_t) * lib->_func_list_size);
  } else if (lib->_func_list_size <= (lib->_func_count + 1)) {
    lib->_func_list_size += TABLE_GROW_SIZE;
    lib->_func_list = (ext_func_node_t *)
                   realloc(lib->_func_list, sizeof(ext_func_node_t) * lib->_func_list_size);
  }

  lib->_func_list[lib->_func_count].lib_id = lib_id;
  lib->_func_list[lib->_func_count].symbol_index = 0;
  strlcpy(lib->_func_list[lib->_func_count].name, func_name, sizeof(lib->_func_list[lib->_func_count].name));
  strupper(lib->_func_list[lib->_func_count].name);

  if (opt_verbose) {
    log_printf("LIB: %d, Idx: %d, FUNC '%s'\n", lib_id, lib->_func_count,
               lib->_func_list[lib->_func_count].name);
  }
  lib->_func_count++;
  return lib->_func_count - 1;
}

static void slib_import_routines(slib_t *lib, int comp) {
  int total = 0;
  char buf[SB_KEYWORD_SIZE];

  lib->_sblib_func_exec = slib_getoptptr(lib, "sblib_func_exec");
  lib->_sblib_proc_exec = slib_getoptptr(lib, "sblib_proc_exec");
  sblib_count_fn fcount = slib_getoptptr(lib, "sblib_proc_count");
  sblib_getname_fn fgetname = slib_getoptptr(lib, "sblib_proc_getname");

  if (fcount && fgetname) {
    int count = fcount();
    total += count;
    for (int i = 0; i < count; i++) {
      if (fgetname(i, buf)) {
        strupper(buf);
        if (!lib->_imported && slib_add_external_proc(buf, lib->_id) == -1) {
          break;
        } else if (comp) {
          char name[NAME_SIZE];
          strlcpy(name, lib->_name, sizeof(name));
          strlcat(name, ".", sizeof(name));
          strlcat(name, buf, sizeof(name));
          strupper(name);
          comp_add_external_proc(name, lib->_id);
        }
      }
    }
  }

  fcount = slib_getoptptr(lib, "sblib_func_count");
  fgetname = slib_getoptptr(lib, "sblib_func_getname");

  if (fcount && fgetname) {
    int count = fcount();
    total += count;
    for (int i = 0; i < count; i++) {
      if (fgetname(i, buf)) {
        strupper(buf);
        if (!lib->_imported && slib_add_external_func(buf, lib->_id) == -1) {
          break;
        } else if (comp) {
          char name[NAME_SIZE];
          strlcpy(name, lib->_name, sizeof(name));
          strlcat(name, ".", sizeof(name));
          strlcat(name, buf, sizeof(name));
          strupper(name);
          comp_add_external_func(name, lib->_id);
        }
      }
    }
  }

  if (!total) {
    log_printf("LIB: module '%s' has no exports\n", lib->_name);
  }
}

//
// whether name ends with LIB_EXT and does not contain '-', eg libstdc++-6.dll
//
static int slib_is_module(const char *name) {
  int result = 0;
  if (name && name[0] != '\0') {
    int offs = strlen(name) - (sizeof(LIB_EXT) - 1);
    result = offs > 0 && strchr(name, '-') == NULL && strcasecmp(name + offs, LIB_EXT) == 0;
  }
  return result;
}

//
// opens the library
//
static void slib_open(const char *fullname, const char *name) {
  int name_index = 0;

  if (strncmp(name, "lib", 3) == 0) {
    // libmysql -> store mysql
    name_index = 3;
  }

  int id = 0; // fixme
  slib_t *lib = plugins[id]; 
  memset(lib, 0, sizeof(slib_t));
  strlcpy(lib->_name, name + name_index, NAME_SIZE);
  strlcpy(lib->_fullname, fullname, PATH_SIZE);
  lib->_id = id;
  lib->_imported = 0;

  if (!opt_quiet) {
    log_printf("LIB: registering '%s'", fullname);
  }
  if (slib_llopen(lib)) {
    // override default name
    sblib_get_module_name_fn get_module_name = slib_getoptptr(lib, "sblib_get_module_name");
    if (get_module_name) {
      strlcpy(lib->_name, get_module_name(), NAME_SIZE);
    }
  } else {
    sc_raise("LIB: can't open %s", fullname);
  }
}

//
//
//
static void slib_open_path(const char *path, const char *name) {
  if (slib_is_module(name)) {
    // ends with LIB_EXT
    char full[PATH_SIZE];
    char libname[NAME_SIZE];

    // copy name without extension
    strlcpy(libname, name, sizeof(libname));
    char *p = strchr(libname, '.');
    *p = '\0';

    // copy full path to name
    strlcpy(full, path, sizeof(full));
    if (path[strlen(path) - 1] != '/') {
      // add trailing separator
      strlcat(full, "/", sizeof(full));
    }
    strlcat(full, name, sizeof(full));
    slib_open(full, libname);
  }
}

//
// build parameter table
//
static int slib_build_ptable(slib_par_t *ptable) {
  int pcount = 0;
  var_t *arg;
  bcip_t ofs;

  if (code_peek() == kwTYPE_LEVEL_BEGIN) {
    code_skipnext();
    byte ready = 0;
    do {
      byte code = code_peek();
      switch (code) {
      case kwTYPE_EOC:
        code_skipnext();
        break;
      case kwTYPE_SEP:
        code_skipsep();
        break;
      case kwTYPE_LEVEL_END:
        ready = 1;
        break;
      case kwTYPE_VAR:
        // variable
        ofs = prog_ip;
        if (code_isvar()) {
          // push parameter
          ptable[pcount].var_p = code_getvarptr();
          ptable[pcount].byref = 1;
          pcount++;
          break;
        }

        // restore IP
        prog_ip = ofs;
        // no 'break' here
      default:
        // default --- expression (BYVAL ONLY)
        arg = v_new();
        eval(arg);
        if (!prog_error) {
          // push parameter
          ptable[pcount].var_p = arg;
          ptable[pcount].byref = 0;
          pcount++;
        } else {
          v_free(arg);
          v_detach(arg);
          return pcount;
        }
      }
      if (pcount == MAX_PARAM) {
        err_parm_limit(MAX_PARAM);
      }
    } while (!ready && !prog_error);
    // kwTYPE_LEVEL_END
    code_skipnext();
  }
  return pcount;
}

//
// free parameter table
//
static void slib_free_ptable(slib_par_t *ptable, int pcount) {
  for (int i = 0; i < pcount; i++) {
    if (ptable[i].byref == 0) {
      v_free(ptable[i].var_p);
      v_detach(ptable[i].var_p);
    }
  }
}

//
// execute a function or procedure
//
static int slib_exec(slib_t *lib, var_t *ret, int index, int proc) {
  slib_par_t *ptable;
  int pcount;
  if (code_peek() == kwTYPE_LEVEL_BEGIN) {
    ptable = (slib_par_t *)malloc(sizeof(slib_par_t) * MAX_PARAM);
    pcount = slib_build_ptable(ptable);
  } else {
    ptable = NULL;
    pcount = 0;
  }
  if (prog_error) {
    slib_free_ptable(ptable, pcount);
    free(ptable);
    return 0;
  }

  int success;
  v_init(ret);
  if (proc) {
    success = lib->_sblib_proc_exec(index, pcount, ptable, ret);
  } else {
    success = lib->_sblib_func_exec(index, pcount, ptable, ret);
  }

  // error
  if (!success) {
    if (ret->type == V_STR) {
      err_throw("LIB:%s: %s\n", lib->_name, ret->v.p.ptr);
    } else {
      err_throw("LIB:%s: Unspecified error calling %s\n", lib->_name, (proc ? "SUB" : "FUNC"));
    }
  }

  // clean-up
  if (ptable) {
    slib_free_ptable(ptable, pcount);
    free(ptable);
  }

  return success;
}

void plugin_init() {
  for (int i = 0; i < MAX_SLIBS; i++) {
    plugins[i] = NULL;
  }
}

int plugin_find(const char *file, const char *alias) {
  // TODO fixme
  //strcpy(file, name);
  //strcat(file, ".bas");

  // find in SBASICPATH
  if (getenv("SBASICPATH")) {
    if (sys_search_path(getenv("SBASICPATH"), file, NULL)) {
      return 1;
    }
  }

  // find in program launch directory
  if (gsb_bas_dir[0] && sys_search_path(gsb_bas_dir, file, NULL)) {
    return 1;
  }

  // TODO find in opt_modpath
  
  // find in current directory
  if (sys_search_path(".", file, NULL)) {
    return 1;
  }

  // create corresponding sbu path version
  //strcpy(unitname, bas_file);

  //if (strcmp(comp_file_name, bas_file) == 0) {
    // unit and program are the same
  //  return -1;
  //}
  return -1;
}

void plugin_import(int lib_id) {
  // TODO fixme
  slib_t *lib = get_lib(lib_id);
  if (lib && !lib->_imported) {
    slib_import_routines(lib, 1);
    lib->_imported = 1;
  }
}

void plugin_open(const char *name, int lib_id) {
  // TODO fixme
  slib_t *lib = get_lib(lib_id);
  if (lib && !lib->_imported) {
    slib_import_routines(lib, 0);
    lib->_imported = 1;
  }
  if (lib) {
    sblib_init_fn minit = slib_getoptptr(lib, "sblib_init");
    if (minit && !minit(gsb_last_file)) {
      rt_raise("LIB: %s->sblib_init(), failed", lib->_name);
    }
  }
}

int plugin_get_kid(int lib_id, const char *name) {
  slib_t *lib = get_lib(lib_id);
  if (lib != NULL) {
    const char *dot = strchr(name, '.');
    const char *field = (dot != NULL ? dot + 1 : name);
    for (int i = 0; i < lib->_proc_count; i++) {
      if (lib->_proc_list[i].lib_id == lib_id &&
          strcmp(lib->_proc_list[i].name, field) == 0) {
        return i;
      }
    }
    for (int i = 0; i < lib->_func_count; i++) {
      if (lib->_func_list[i].lib_id == lib_id &&
          strcmp(lib->_func_list[i].name, field) == 0) {
        return i;
      }
    }
  }
  return -1;
}

void *plugin_get_func(const char *name) {
  void *result = NULL;
  for (int i = 0; i < MAX_SLIBS && result == NULL; i++) {
    if (plugins[i]) {
      slib_t *lib = plugins[i];
      if (lib->_imported) {
        result = slib_getoptptr(lib, name);
      }
    }
  }
  return result;
}

int plugin_procexec(int lib_id, int index) {
  int result;
  slib_t *lib = get_lib(lib_id);
  if (lib && lib->_sblib_proc_exec) {
    var_t ret;
    v_init(&ret);
    result = slib_exec(lib, &ret, index, 1);
    v_free(&ret);
  } else {
    result = 0;
  }
  return result;
}

int plugin_funcexec(int lib_id, int index, var_t *ret) {
  int result;
  slib_t *lib = get_lib(lib_id);
  if (lib && lib->_sblib_func_exec) {
    result = slib_exec(lib, ret, index, 0);
  } else {
    result = 0;
  }
  return result;
}

void plugin_close() {
  for (int i = 0; i < MAX_SLIBS; i++) {
    if (plugins[i]) {
      slib_t *lib = plugins[i];
      if (lib->_handle) {
        sblib_close_fn mclose = slib_getoptptr(lib, "sblib_close");
        if (mclose) {
          mclose();
        }
        slib_llclose(lib);
      }
      free(lib->_proc_list);
      free(lib->_func_list);
      free(lib);
    }
    plugins[i] = NULL;
  }
}

#else
// dummy implementations
int plugin_find(const char *file, char *alias) { return -1; }
int plugin_funcexec(int lib_id, int index, var_t *ret) { return -1; }
int plugin_get_kid(int lib_id, const char *keyword) { return -1; }
int plugin_procexec(int lib_id, int index) { return -1; }
void *plugin_get_func(const char *name) { return 0; }
void plugin_close() {}
void plugin_import(int lib_id) {}
void plugin_init() {}
void plugin_open(const char *name, int lib_id) {}
#endif
