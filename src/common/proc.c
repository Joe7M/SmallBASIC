// This file is part of SmallBASIC
//
// SmallBASIC, library API
//
// This program is distributed under the terms of the GPL v2.0 or later
// Download the GNU Public License (GPL) from www.gnu.org
//
// Copyright(C) 2000 Nicholas Christopoulos

#include "common/sys.h"
#include "common/pproc.h"
#include "common/messages.h"
#include "common/inet.h"

#include <limits.h>
#include <dirent.h>

#define MAX_PARAM 8

/*
 * returns the last-modified time of the file
 *
 * on error returns 0L
 */
time_t sys_filetime(const char *file) {
  struct stat st;

  if (stat(file, &st) == 0) {
    return st.st_mtime;
  }
  return 0L;
}

/*
 * search a set of directories for the given file
 * directories on path must be separated with symbol ':' (unix) or ';' (dos/win)
 *
 * @param path the path
 * @param file the file
 * @param retbuf a buffer to store the full-path-name file (can be NULL)
 * @return non-zero if found
 */
int sys_search_path(const char *path, const char *file, char *retbuf) {
  char cur_path[OS_PATHNAME_SIZE + 1];
  int found = 0;

  if (path == NULL || strlen(path) == 0) {
    return 0;
  }
  const char *ps = path;
  const char *p;
  do {
    // next element, build cur_path
    p = strchr(ps, ':');
    if (!p) {
      strlcpy(cur_path, ps, sizeof(cur_path));
    } else {
      strncpy(cur_path, ps, p - ps);
      cur_path[p - ps] = '\0';
      ps = p + 1;
    }

    // fix home directory
    if (cur_path[0] == '~') {
      char *old_path = malloc(strlen(cur_path));
      strcpy(old_path, cur_path + 1);
      if (getenv("HOME")) {
        strlcpy(cur_path, getenv("HOME"), sizeof(cur_path));
      } else {
        strlcpy(cur_path, getenv("HOMEDRIVE"), sizeof(cur_path));
        strlcat(cur_path, getenv("HOMEPATH"), sizeof(cur_path));
      }
      if (old_path[0] != '/') {
        strlcat(cur_path, "/", sizeof(cur_path));
      }
      strlcat(cur_path, old_path, sizeof(cur_path));
      free(old_path);
    }

    DIR *dp = opendir(cur_path);
    if (dp != NULL) {
      struct dirent *entry;
      while (!found && (entry = readdir(dp)) != NULL) {
        if (strcasecmp(entry->d_name, file) == 0) {
          int end = strlen(cur_path);
          strcat(cur_path, "/");
          strcat(cur_path, entry->d_name);
          if (access(cur_path, R_OK) == 0) {
            if (retbuf) {
              strcpy(retbuf, cur_path);
            }
            found = 1;
          } else {
            cur_path[end] = '\0';
          }
        }
      }
      closedir(dp);
    }
  } while (p && !found);
  return found;
}

/*
 * execute a user's expression (using one variable)
 * (note: keyword USE)
 *
 * var - the variable (the X)
 * ip  - expression's address
 */
void exec_usefunc(var_t *var, bcip_t ip) {
  // save X
  var_t *old_x = v_clone(tvar[SYSVAR_X]);

  // run
  v_set(tvar[SYSVAR_X], var);
  v_free(var);
  code_jump(ip);
  eval(var);

  // restore X
  v_set(tvar[SYSVAR_X], old_x);
  v_free(old_x);
  v_detach(old_x);
}

/*
 * execute a user's expression (using two variable)
 *
 * var1 - the first variable (the X)
 * var2 - the second variable (the Y)
 * ip   - expression's address
 */
void exec_usefunc2(var_t *var1, var_t *var2, bcip_t ip) {
  // save X
  var_t *old_x = v_clone(tvar[SYSVAR_X]);
  var_t *old_y = v_clone(tvar[SYSVAR_Y]);

  // run
  v_set(tvar[SYSVAR_X], var1);
  v_free(var1);
  v_set(tvar[SYSVAR_Y], var2);
  v_free(var2);
  code_jump(ip);
  eval(var1);

  // restore X,Y
  v_set(tvar[SYSVAR_X], old_x);
  v_free(old_x);
  v_detach(old_x);
  v_set(tvar[SYSVAR_Y], old_y);
  v_free(old_y);
  v_detach(old_y);
}

void pv_write_str(char *str, var_t *vp) {
  vp->v.p.length += strlen(str);
  if (vp->v.p.ptr == NULL) {
    vp->v.p.ptr = malloc(vp->v.p.length + 1);
    vp->v.p.owner = 1;
    strcpy(vp->v.p.ptr, str);
  } else {
    vp->v.p.ptr = realloc(vp->v.p.ptr, vp->v.p.length + 1);
    strcat(vp->v.p.ptr, str);
  }
}

void pv_write_str_var(var_t *var, int method, intptr_t handle) {
  switch (method) {
  case PV_FILE:
    dev_fwrite((int)handle, (byte *)var->v.p.ptr, var->v.p.length - 1);
    break;
  case PV_LOG:
    lwrite(var->v.p.ptr);
    break;
  case PV_STRING:
    pv_write_str(var->v.p.ptr, (var_t *)handle);
    break;
  case PV_NET:
    net_send((socket_t)handle, (const char *)var->v.p.ptr, var->v.p.length - 1);
    break;
  default:
    dev_print(var->v.p.ptr);
  }
}

/*
 * Write string to output device
 */
void pv_write(char *str, int method, intptr_t handle) {
  switch (method) {
  case PV_FILE:
    dev_fwrite((int)handle, (byte *)str, strlen(str));
    break;
  case PV_LOG:
    lwrite(str);
    break;
  case PV_STRING:
    pv_write_str(str, (var_t *)handle);
    break;
  case PV_NET:
    net_print((socket_t)handle, (const char *)str);
    break;
  default:
    dev_print(str);
  }
}

/*
 * just prints the value of variable 'var'
 */
void pv_writevar(var_t *var, int method, intptr_t handle) {
  char tmpsb[64];

  // start with a clean buffer
  memset(tmpsb, 0, sizeof(tmpsb));

  switch (var->type) {
  case V_INT:
    ltostr(var->v.i, tmpsb);
    pv_write(tmpsb, method, handle);
    break;
  case V_NUM:
    ftostr(var->v.n, tmpsb);
    pv_write(tmpsb, method, handle);
    break;
  case V_STR:
    pv_write_str_var(var, method, handle);
    break;
  case V_ARRAY:
  case V_MAP:
    map_write(var, method, handle);
    break;
  case V_PTR:
    ltostr(var->v.ap.p, tmpsb);
    pv_write(tmpsb, method, handle);
    break;
  case V_REF:
    pv_writevar(var->v.ref, method, handle);
    break;
  case V_NIL:
    pv_write(SB_KW_NONE_STR, method, handle);
    break;
  }
}

/*
 * write a variable to console
 */
void print_var(var_t *v) {
  pv_writevar(v, PV_CONSOLE, 0);
}

/*
 * write a variable to a file
 */
void fprint_var(intptr_t handle, var_t *v) {
  pv_writevar(v, PV_FILE, handle);
}

/*
 * write a variable to log-file
 */
void logprint_var(var_t *v) {
  pv_writevar(v, PV_LOG, 0);
}

/*
 * skip parameter
 */
void par_skip() {
  byte exitf = 0;
  uint32_t len;
  int level = 0;

  do {
    switch (code_peek()) {
    case kwTYPE_INT:           // integer
      prog_ip += OS_INTSZ + 1;
      break;
    case kwTYPE_NUM:           // number
      prog_ip += OS_REALSZ + 1;
      break;
    case kwTYPE_STR:           // string: [2/4B-len][data]
      prog_ip++;
      memcpy(&len, prog_source + prog_ip, OS_STRLEN);
      len += OS_STRLEN;
      prog_ip += len;
      break;
    case kwTYPE_VAR:           // [addr|id]
      prog_ip += ADDRSZ + 1;
      break;
    case kwTYPE_CALLF:
      prog_ip += CODESZ + 1;
      break;
    case kwTYPE_CALLEXTF:
      prog_ip += (ADDRSZ * 2) + 1;
      break;
    case kwTYPE_LEVEL_BEGIN:   // left parenthesis
      level++;
      prog_ip++;
      break;
    case kwTYPE_LEVEL_END:     // right parenthesis
      prog_ip++;
      level--;
      if (level <= 0)
        exitf = 1;
      break;
    case kwTYPE_LOGOPR:
    case kwTYPE_CMPOPR:
    case kwTYPE_ADDOPR:
    case kwTYPE_MULOPR:
    case kwTYPE_POWOPR:
    case kwTYPE_UNROPR:        // [1B data]
      prog_ip += 2;
      break;
    case kwTYPE_CALL_UDF:      // [true-ip][false-ip]
      prog_ip += BC_CTRLSZ + 1;
      break;
    case kwTYPE_LINE:
    case kwTYPE_EOC:
    case kwUSE:
      if (level != 0) {
        rt_raise("Block error!");
      }
      exitf = 1;
      break;
    case kwTYPE_SEP:
      if (level <= 0) {
        exitf = 1;
      } else {
        prog_ip += 2;
      }
      break;
    default:
      prog_ip++;
    }
  } while (!exitf);
}

/*
 * get next parameter as var_t
 */
void par_getvar(var_t *var) {
  switch (code_peek()) {
  case kwTYPE_LINE:
  case kwTYPE_EOC:
  case kwTYPE_SEP:
    err_syntax(-1, "%P");
    return;
  default:
    eval(var);
    break;
  };
}

/*
 * get next parameter as var_t/array
 */
var_t *par_getvarray() {
  var_t *var;

  switch (code_peek()) {
  case kwTYPE_LINE:
  case kwTYPE_EOC:
  case kwTYPE_SEP:
    err_syntax(-1, "%P");
    return NULL;
  case kwTYPE_VAR:
    var = code_getvarptr();
    if (!prog_error) {
      if (var->type != V_ARRAY) {
        return NULL;
      }
    }
    return var;
  default:
    err_typemismatch();
    break;
  };
  return NULL;
}

/*
 * get next parameter as var_t
 */
var_t *par_getvar_ptr() {
  if (code_peek() != kwTYPE_VAR) {
    err_syntax(-1, "%P");
    return NULL;
  }
  return code_getvarptr();
}

/*
 * get next parameter as var_t
 */
void par_getstr(var_t *var) {
  switch (code_peek()) {
  case kwTYPE_LINE:
  case kwTYPE_EOC:
  case kwTYPE_SEP:
    err_syntax(-1, "%S");
    return;
  default:
    v_init(var);
    eval(var);
    break;
  };

  if (var->type != V_STR) {
    v_tostr(var);
  }
}

/*
 * get next parameter as long
 */
var_int_t par_getint() {
  var_t var;

  v_init(&var);
  par_getvar(&var);
  var_int_t i = v_getint(&var);
  v_free(&var);

  return i;
}

/*
 * get next parameter as double
 */
var_num_t par_getnum() {
  var_t var;

  v_init(&var);
  par_getvar(&var);
  var_num_t f = v_getval(&var);
  v_free(&var);

  return f;
}

var_int_t par_next_int(int sep) {
  var_int_t result;
  if (prog_error) {
    result = 0;
  } else {
    result = par_getint();
  }
  if (!prog_error && (sep || code_peek() == kwTYPE_SEP)) {
    par_getcomma();
  }
  return result;
}

var_t *par_next_str(var_t *arg, int sep) {
  var_t *result;
  if (prog_error) {
    result = NULL;
  } else if (code_isvar()) {
    result = code_getvarptr();
  } else {
    eval(arg);
    result = arg;
    if (result->type != V_STR) {
      v_tostr(arg);
    }
  }
  if (!prog_error && (sep || code_peek() == kwTYPE_SEP)) {
    par_getcomma();
  }
  return result;
}

var_int_t par_getval(var_int_t def) {
  var_int_t result;
  if (prog_error) {
    result = def;
  } else {
    var_t var;
    switch (code_peek()) {
    case kwTYPE_LINE:
    case kwTYPE_EOC:
    case kwTYPE_SEP:
    case kwTYPE_LEVEL_END:
    case kwTYPE_EVPUSH:
      result = def;
      break;
    default:
      v_init(&var);
      eval(&var);
      result = prog_error ? 0 : v_getint(&var);
      v_free(&var);
      break;
    }
  }
  return result;
}

/*
 * no-error if the next byte is separator
 * returns the separator
 */
int par_getsep() {
  switch (code_peek()) {
  case kwTYPE_SEP:
    code_skipnext();
    return code_getnext();
  default:
    err_missing_sep();
  };

  return 0;
}

/*
 * no-error if the next byte is the separator ','
 */
void par_getcomma() {
  if (par_getsep() != ',') {
    if (!prog_error) {
      err_missing_comma();
    }
  }
}

/*
 * no-error if the next byte is the separator ';'
 */
void par_getsemicolon() {
  if (par_getsep() != ';') {
    if (!prog_error) {
      err_syntaxsep(";");
    }
  }
}

/*
 * no-error if the next byte is the separator '#'
 */
void par_getsharp() {
  if (par_getsep() != '#') {
    if (!prog_error) {
      err_syntaxsep("#");
    }
  }
}

/*
 * retrieve a 2D point (double)
 */
pt_t par_getpt() {
  pt_t pt;
  var_t *var;
  byte alloc = 0;

  pt.x = pt.y = 0;

  // first parameter
  if (code_isvar()) {
    var = code_getvarptr();
  } else {
    alloc = 1;
    var = v_new();
    eval(var);
  }

  if (!prog_error) {
    if (var->type == V_ARRAY) {
      // array
      if (v_asize(var) != 2) {
        rt_raise(ERR_POLY_POINT);
      } else {
        pt.x = v_getreal(v_elem(var, 0));
        pt.y = v_getreal(v_elem(var, 1));
      }
    } else {
      // non-arrays
      pt.x = v_getreal(var);
      par_getcomma();
      if (!prog_error) {
        var_t v2;
        v_init(&v2);
        eval(&v2);
        if (!prog_error) {
          pt.y = v_getreal(&v2);
        }
        v_free(&v2);
      }
    }
  }
  // clean-up
  if (alloc) {
    v_free(var);
    v_detach(var);
  }

  return pt;
}

/*
 * retrieve a 2D point (integer)
 */
ipt_t par_getipt() {
  ipt_t pt;
  var_t *var;
  byte alloc = 0;

  pt.x = pt.y = 0;

  // first parameter
  if (code_isvar()) {
    var = code_getvarptr();
  } else {
    alloc = 1;
    var = v_new();
    eval(var);
  }

  if (!prog_error) {
    if (var->type == V_ARRAY) {
      // array
      if (v_asize(var) != 2) {
        rt_raise(ERR_POLY_POINT);
      } else {
        pt.x = v_getint(v_elem(var, 0));
        pt.y = v_getint(v_elem(var, 1));
      }
    } else {
      // non-arrays
      pt.x = v_getint(var);
      par_getcomma();
      if (!prog_error) {
        var_t v2;
        v_init(&v2);
        eval(&v2);
        if (!prog_error) {
          pt.y = v_getint(&v2);
        }
        v_free(&v2);
      }
    }
  }
  // clean-up
  if (alloc) {
    v_free(var);
    v_detach(var);
  }

  return pt;
}

/*
 * retrieve a 2D polyline
 */
int par_getpoly(pt_t **poly_pp) {
  pt_t *poly = NULL;
  var_t *var, *el;
  int count = 0;
  byte style = 0, alloc = 0;

  // get array
  if (code_isvar()) {
    var = par_getvarray();
    if (var == NULL || prog_error) {
      return 0;
    }
  } else {
    var = v_new();
    eval(var);
    alloc = 1;
  }

  // zero-length or non array
  if (var->type != V_ARRAY || v_asize(var) == 0) {
    if (alloc) {
      v_free(var);
      v_detach(var);
    }
    return 0;
  }

  el = v_elem(var, 0);
  if (el->type == V_ARRAY) {
    style = 1;                  // nested --- [ [x1,y1], [x2,y2], ... ]
  }
  // else style = 0; // 2x2 or 1x --- [ x1, y1, x2, y2, ... ]

  // error check
  if (style == 1) {
    if (v_asize(el) != 2) {
      err_parsepoly(-1, 1);
      if (alloc) {
        v_free(var);
        v_detach(var);
      }
      return 0;
    }

    count = v_asize(var);
  } else if (style == 0) {
    if ((v_asize(var) % 2) != 0) {
      err_parsepoly(-1, 2);
      if (alloc) {
        v_free(var);
        v_detach(var);
      }
      return 0;
    }

    count = v_asize(var) >> 1;
  }
  // build array
  *poly_pp = poly = malloc(sizeof(pt_t) * count);

  if (style == 1) {
    int i;

    for (i = 0; i < count; i++) {
      // get point
      el = v_elem(var, i);

      // error check
      if (el->type != V_ARRAY) {
        err_parsepoly(i, 3);
      } else if (v_asize(el) != 2) {
        err_parsepoly(i, 4);
      }
      if (prog_error) {
        break;
      }
      // store point
      poly[i].x = v_getreal(v_elem(el, 0));
      poly[i].y = v_getreal(v_elem(el, 1));
    }
  } else if (style == 0) {
    int i, j;

    for (i = j = 0; i < count; i++, j += 2) {
      // error check
      if (prog_error) {
        break;
      }
      // store point
      poly[i].x = v_getreal(v_elem(var, j));
      poly[i].y = v_getreal(v_elem(var, j + 1));
    }
  }
  // clean-up
  if (prog_error) {
    free(poly);
    *poly_pp = NULL;
    count = 0;
  }
  if (alloc) {
    v_free(var);
    v_detach(var);
  }

  return count;
}

/*
 * retrieve a 2D polyline (integers)
 */
int par_getipoly(ipt_t **poly_pp) {
  ipt_t *poly = NULL;
  var_t *var, *el;
  int count = 0;
  byte style = 0, alloc = 0;

  // get array
  if (code_isvar()) {
    var = par_getvarray();
    if (var == NULL || prog_error) {
      return 0;
    }
  } else {
    var = v_new();
    eval(var);
    alloc = 1;
  }

  // zero-length or non array
  if (var->type != V_ARRAY || v_asize(var) == 0) {
    if (alloc) {
      v_free(var);
      v_detach(var);
    }
    return 0;
  }
  //
  el = v_elem(var, 0);
  if (el && el->type == V_ARRAY) {
    style = 1;   // nested --- [ [x1,y1], [x2,y2], ... ]
  }
  // else
  // style = 0; // 2x2 or 1x --- [ x1, y1, x2, y2, ... ]

  // error check
  if (style == 1) {
    if (v_asize(el) != 2) {
      err_parsepoly(-1, 1);
      if (alloc) {
        v_free(var);
        v_detach(var);
      }
      return 0;
    }

    count = v_asize(var);
  } else if (style == 0) {
    if ((v_asize(var) % 2) != 0) {
      err_parsepoly(-1, 2);
      if (alloc) {
        v_free(var);
        v_detach(var);
      }
      return 0;
    }

    count = v_asize(var) >> 1;
  }
  // build array
  *poly_pp = poly = malloc(sizeof(ipt_t) * count);

  if (style == 1) {
    int i;

    for (i = 0; i < count; i++) {
      // get point
      el = v_elem(var, i);

      // error check
      if (el->type != V_ARRAY) {
        err_parsepoly(i, 3);
      } else if (v_asize(el) != 2) {
        err_parsepoly(i, 4);
      }
      if (prog_error) {
        break;
      }
      // store point
      poly[i].x = v_getint(v_elem(el, 0));
      poly[i].y = v_getint(v_elem(el, 1));
    }
  } else if (style == 0) {
    int i, j;

    for (i = j = 0; i < count; i++, j += 2) {
      // error check
      if (prog_error) {
        break;
      }
      // store point
      poly[i].x = v_getint(v_elem(var, j));
      poly[i].y = v_getint(v_elem(var, j + 1));
    }
  }
  // clean-up
  if (prog_error) {
    free(poly);
    *poly_pp = NULL;
    count = 0;
  }
  if (alloc) {
    v_free(var);
    v_detach(var);
  }

  return count;
}

/*
 * destroy a parameter-table which was created by par_getparlist
 */
void par_freepartable(par_t **ptable_pp, int pcount) {
  par_t *ptable = *ptable_pp;
  if (ptable) {
    for (int i = 0; i < pcount; i++) {
      if (ptable[i].flags & PAR_BYVAL) {
        v_free(ptable[i].var);
        v_detach(ptable[i].var);
      }
    }
    free(ptable);
  }
  *ptable_pp = NULL;
}

/*
 * builds a parameter table
 *
 * ptable_pp = pointer to an ptable
 * valid_sep = valid separators (';)
 *
 * returns the number of the parameters, OR, -1 on error
 *
 * YOU MUST FREE THAT TABLE BY USING par_freepartable()
 * IF THERE IS NO ERROR, CALL TO par_freepartable IS NOT NEEDED
 */
int par_getpartable(par_t **ptable_pp, const char *valid_sep) {
  bcip_t ofs;
  char vsep[8];

  // initialize
  var_t *par = NULL;
  byte last_sep = 0;
  int pcount = 0;
  par_t *ptable = *ptable_pp = malloc(sizeof(par_t) * MAX_PARAM);

  if (valid_sep) {
    strlcpy(vsep, valid_sep, sizeof(vsep));
  } else {
    strlcpy(vsep, ",", sizeof(vsep));
  }

  // start
  byte ready = 0;
  do {
    switch (code_peek()) {
    case kwTYPE_EOC:
    case kwTYPE_LINE:
    case kwUSE:
    case kwTYPE_LEVEL_END:     // end of parameters
      ready = 1;
      break;
    case kwTYPE_SEP:           // separator
      code_skipnext();

      // check parameters separator
      if (strchr(vsep, (last_sep = code_getnext())) == NULL) {
        if (strlen(vsep) <= 1) {
          err_missing_comma();
        } else {
          err_syntaxsep(vsep);
        }
        par_freepartable(ptable_pp, pcount);
        return -1;
      }
      // update par.next_sep
      if (pcount) {
        ptable[pcount - 1].next_sep = last_sep;
      }
      break;
    case kwTYPE_VAR:           // variable
      ofs = prog_ip;            // store IP

      ptable[pcount].flags = 0;
      ptable[pcount].prev_sep = last_sep;
      ptable[pcount].next_sep = 0;

      if (code_isvar()) {
        // push parameter
        ptable[pcount].var = code_getvarptr();
        if (++pcount == MAX_PARAM) {
          par_freepartable(ptable_pp, pcount);
          err_parfmt(__FILE__);
          return -1;
        }
        break;
      }
      // Its no a single variable, its an expression
      // restore IP
      prog_ip = ofs;

      // no 'break' here
    default:
      // default --- expression (BYVAL ONLY)
      par = v_new();
      eval(par);
      if (!prog_error) {
        // push parameter
        ptable[pcount].var = par;
        ptable[pcount].flags = PAR_BYVAL;
        if (++pcount == MAX_PARAM) {
          par_freepartable(ptable_pp, pcount);
          err_parfmt(__FILE__);
          return -1;
        }
      } else {
        v_free(par);
        v_detach(par);
        par_freepartable(ptable_pp, pcount);
        return -1;
      }
    }

  } while (!ready);

  return pcount;
}

/*
 */
int par_massget_type_check(char fmt, par_t *par) {
  switch (fmt) {
  case 'S':
  case 's':
    return (par->var->type == V_STR);
  case 'I':
  case 'i':
  case 'F':
  case 'f':
    return (par->var->type == V_NUM || par->var->type == V_INT);
  }
  return 0;
}

/*
 * Parsing parameters with scanf-style
 * returns the parameter-count or -1 (error)
 *
 * Format:
 * --------
 * capital character = the parameter is required
 * small character   = optional parameter
 *
 * I = var_int_t       (var_int_t*)
 * F = var_num_t       (var_num_t*)
 * S = string          (char*)
 * P = variable's ptr  (var_t*)
 *
 * Example:
 * --------
 * var_int_t i1, i2 = -1;    // -1 is the default value for i2
 * char      *s1 = NULL;     // NULL is the default value for s1
 * var_t     *v  = NULL;     // NULL is the default value for v
 *
 * // the first integer is required, the second is optional
 * // the string is optional too
 * pc = par_massget("Iis", &i1, &i2, &s1, &v);
 *
 * if ( pc != -1 ) {
 *   // no error; also, you can use prog_error because par_massget() will call rt_raise() on error
 *   printf("required integer = %d\n", i1);
 *
 *   // if there is no optional parameters, the default value will be returned
 *   if  ( i2 != -1 )    printf("optional integer found = %d\n", i2);
 *   if  ( s1 )          printf("optional string found = %s\n", s1);
 *   if  ( v )       {   printf("optional variable's ptr found");    v_free(v);  }
 *   }
 *
 * pfree2(s1, v);
 */
int par_massget(const char *fmt, ...) {
  // get ptable
  par_t *ptable;
  int pcount = par_getpartable(&ptable, NULL);
  if (pcount == -1) {
    free(ptable);
    return -1;
  }

  // count pars
  char *fmt_p = (char *)fmt;
  int optcount = 0;
  int rqcount = 0;
  while (*fmt_p) {
    if (*fmt_p >= 'a') {
      optcount++;
    } else {
      rqcount++;
    }
    fmt_p++;
  }

  if (rqcount > pcount) {
    err_parfmt(fmt);
  }
  else {
    // parse
    int curpar = 0;
    int opt = 0;
    int ignore = 0;
    va_list ap;

    va_start(ap, fmt);
    fmt_p = (char *)fmt;
    while (*fmt_p) {
      if (*fmt_p >= 'a' && optcount &&
          ((curpar < pcount) ? par_massget_type_check(*fmt_p, &ptable[curpar]) : 0)) {
        (optcount--, opt = 1, ignore = 0);
      }
      else if (*fmt_p >= 'a' && optcount) {
        (optcount--, opt = 0, ignore = 1);
      }
      else if (*fmt_p < 'a' && rqcount) {
        (rqcount--, opt = 0, ignore = 0);
      }
      else {
        err_parfmt(fmt);
        break;
      }

      if (pcount <= curpar && ignore == 0) {
        err_parfmt(fmt);
        break;
      }

      char **s;
      var_int_t *i;
      var_num_t *f;
      var_t **vt;

      switch (*fmt_p) {
      case 's':
        // optional string
        if (!opt) {
          // advance ap to next position
          va_arg(ap, char **);
          break;
        }
      case 'S':
        // string
        s = va_arg(ap, char **);
        *s = strdup(v_getstr(ptable[curpar].var));
        curpar++;
        break;
      case 'i':
        // optional integer
        if (!opt) {
          va_arg(ap, var_int_t *);
          break;
        }
      case 'I':
        // integer
        i = va_arg(ap, var_int_t *);
        *i = v_getint(ptable[curpar].var);
        curpar++;
        break;
      case 'f':
        // optional real (var_num_t)
        if (!opt) {
          va_arg(ap, var_num_t *);
          break;
        }
      case 'F':
        // real (var_num_t)
        f = va_arg(ap, var_num_t *);
        *f = v_getreal(ptable[curpar].var);
        curpar++;
        break;
      case 'p':
        // optional variable
        if (!opt) {
          va_arg(ap, var_t **);
          break;
        }
      case 'P':
        // variable
        vt = va_arg(ap, var_t **);
        if (ptable[curpar].flags == 0)// byref
          *vt = ptable[curpar].var;
        else {
          err_syntax(-1, "%P");
          break;
        }
        curpar++;
        break;
      }

      fmt_p++;
    }
    va_end(ap);
  }

  par_freepartable(&ptable, pcount);
  if (prog_error) {
    return -1;
  }
  return pcount;
}
