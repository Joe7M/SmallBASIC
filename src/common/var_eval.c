// This file is part of SmallBASIC
//
// Evaluate variables from bytecode
//
// This program is distributed under the terms of the GPL v2.0 or later
// Download the GNU Public License (GPL) from www.gnu.org
//
// Copyright(C) 2010-2024 Chris Warren-Smith. [http://tinyurl.com/ja2ss]

#include "common/sys.h"
#include "common/kw.h"
#include "common/pproc.h"
#include "common/sberr.h"
#include "common/smbas.h"
#include "common/var.h"
#include "common/var_eval.h"
#include "common/plugins.h"

//
// returns a temporary var that can exist in the calling scope
//
var_p_t v_get_tmp(var_p_t map) {
  var_p_t result = map_get(map, MAP_TMP_FIELD);
  if (result == NULL) {
    result = map_add_var(map, MAP_TMP_FIELD, 0);
  }
  return result;
}

void v_eval_func(var_p_t self, var_p_t v_func, var_p_t result) {
  if (v_func->v.fn.cb != NULL) {
    // internal object method
    if (code_peek() == kwTYPE_LEVEL_BEGIN) {
      code_skipnext();
    }
    v_func->v.fn.cb(self, NULL);
    if (code_peek() == kwTYPE_LEVEL_END) {
      code_skipnext();
    }
  } else {
    // module callback
    slib_par_t *ptable;
    int pcount;
    if (code_peek() == kwTYPE_LEVEL_BEGIN) {
      ptable = (slib_par_t *)malloc(sizeof(slib_par_t) * MAX_PARAMS);
      pcount = plugin_build_ptable(ptable, MAX_PARAMS);
    } else {
      ptable = NULL;
      pcount = 0;
    }
    if (!prog_error) {
      if (!v_func->v.fn.mcb(self, pcount, ptable, result)) {
        if (result->type == V_STR) {
          err_throw("%s", result->v.p.ptr);
        } else {
          err_throw("Undefined");
        }
      }
    }
    if (ptable) {
      plugin_free_ptable(ptable, pcount);
      free(ptable);
    }
  }
}

/**
 * Convert multi-dim index to one-dim index
 */
bcip_t get_array_idx(var_t *array) {
  bcip_t idx = 0;
  bcip_t lev = 0;

  do {
    var_t var;
    v_init(&var);
    eval(&var);

    if (prog_error) {
      break;
    } else if (var.type == V_STR || array->type == V_MAP) {
      err_varnotnum();
      break;
    } else {
      bcip_t idim = v_getint(&var);

      v_free(&var);
      idim = idim - v_lbound(array, lev);

      bcip_t m = idim;
      for (bcip_t i = lev + 1; i < v_maxdim(array); i++) {
        m = m * (ABS(v_ubound(array, i) - v_lbound(array, i)) + 1);
      }
      idx += m;

      // skip separator
      byte code = code_peek();
      if (code == kwTYPE_SEP) {
        code_skipnext();
        if (code_getnext() != ',') {
          err_missing_comma();
        }
      }
      // next
      lev++;
    }
  } while (!prog_error && code_peek() != kwTYPE_LEVEL_END);

  if (!prog_error) {
    if ((int) v_maxdim(array) != lev) {
      err_missing_sep();
    }
  }
  return idx;
}

/**
 * Returns the map field along with the immediate parent map
 */
var_t *code_getvarptr_map(var_t **var_map) {
  var_t *var_p = NULL;
  if (code_peek() == kwTYPE_VAR) {
    code_skipnext();
    *var_map = tvar[code_getaddr()];
    if (code_peek() == kwTYPE_UDS_EL) {
      var_p = map_resolve_fields(*var_map, var_map);
    }
  }
  return var_p;
}

var_t *v_set_self(var_t *map) {
  var_t *self = tvar[SYSVAR_SELF];
  var_t *result = (self->type == V_REF) ? self->v.ref : NULL;
  if (map != NULL) {
    self->const_flag = 0;
    self->type = V_REF;
    self->v.ref = map;
  } else {
    self->const_flag = 1;
    self->type = V_INT;
    self->v.i = 0;
  }
  return result;
}

/**
 * Used by code_getvarptr() to retrieve an element ptr of an array
 */
var_t *code_getvarptr_arridx(var_t *basevar_p) {
  var_t *var_p = NULL;

  if (code_peek() != kwTYPE_LEVEL_BEGIN) {
    err_arrmis_lp();
  } else {
    code_skipnext();
    bcip_t array_index = get_array_idx(basevar_p);
    if (!prog_error) {
      if ((int) array_index < v_asize(basevar_p) && (int) array_index >= 0) {
        var_p = v_elem(basevar_p, array_index);
        if (code_peek() == kwTYPE_LEVEL_END) {
          code_skipnext();
          if (code_peek() == kwTYPE_LEVEL_BEGIN) {
            // there is a second array inside
            if (var_p->type != V_ARRAY) {
              err_varisnotarray();
            } else {
              return code_getvarptr_arridx(var_p);
            }
          }
        } else {
          err_arrmis_rp();
        }
      } else {
        err_arridx(array_index, v_asize(basevar_p));
      }
    }
  }
  return var_p;
}

var_t *code_get_map_element(var_t *map, var_t *field) {
  var_t *result = NULL;

  if (code_peek() != kwTYPE_LEVEL_BEGIN) {
    err_arrmis_lp();
  } else if (field->type == V_PTR) {
    prog_ip = cmd_push_args(kwFUNC, field->v.ap.p, field->v.ap.v);
    var_t *self = v_set_self(map);
    bc_loop(2);
    v_set_self(self);

    if (!prog_error) {
      stknode_t udf_rv;
      code_pop(&udf_rv, kwTYPE_RET);
      if (udf_rv.type != kwTYPE_RET) {
        err_stackmess();
      } else {
        var_p_t var = v_get_tmp(map);
        v_move(var, udf_rv.x.vdvar.vptr);
        v_detach(udf_rv.x.vdvar.vptr);
        result = var;
      }
    }
  } else if (field->type == V_ARRAY) {
    result = code_getvarptr_arridx(field);
  } else if (field->type == V_FUNC) {
    result = v_get_tmp(map);
    v_init(result);
    v_eval_func(map, field, result);
  } else {
    code_skipnext();
    var_t var;
    v_init(&var);
    eval(&var);
    if (!prog_error) {
      map_get_value(field, &var, &result);
      if (code_peek() == kwTYPE_LEVEL_END) {
        code_skipnext();
      } else {
        err_missing_sep();
      }
    }
    v_free(&var);
  }
  return result;
}

/**
 * resolve a composite variable reference, eg: ar.ch(0).foo
 */
var_t *code_resolve_varptr(var_t *var_p, int until_parens) {
  int deref = 1;
  while (deref && var_p != NULL) {
    switch (code_peek()) {
    case kwTYPE_LEVEL_BEGIN:
      if (until_parens) {
        deref = 0;
      } else {
        var_p = code_getvarptr_arridx(var_p);
      }
      break;
    case kwTYPE_UDS_EL:
      var_p = map_resolve_fields(var_p, NULL);
      break;
    default:
      deref = 0;
    }
  }
  return var_p;
}

/**
 * resolve a composite variable reference, eg: ar.ch(0).foo
 */
var_t *code_resolve_map(var_t *var_p, int until_parens) {
  int deref = 1;
  var_t *v_parent = var_p;
  while (deref && var_p != NULL) {
    switch (code_peek()) {
    case kwTYPE_LEVEL_BEGIN:
      if (until_parens) {
        deref = 0;
      } else {
        var_p = code_get_map_element(v_parent, var_p);
      }
      break;
    case kwTYPE_UDS_EL:
      var_p = map_resolve_fields(var_p, &v_parent);
      break;
    default:
      deref = 0;
    }
    var_p = eval_ref_var(var_p);
  }
  return var_p;
}

var_t *resolve_var_ref(var_t *var_p, int *is_ptr) {
  switch (code_peek()) {
  case kwTYPE_LEVEL_BEGIN:
    if (var_p->type == V_PTR) {
      *is_ptr = 1;
    } else {
      var_p = resolve_var_ref(code_getvarptr_arridx(var_p), is_ptr);
    }
    break;
  case kwTYPE_UDS_EL:
    var_p = resolve_var_ref(map_resolve_fields(var_p, NULL), is_ptr);
    break;
  }
  return var_p;
}

/**
 * resolve the map without invoking any V_PTRs
 */
var_t *code_isvar_resolve_map(var_t *var_p, int *is_ptr) {
  int deref = 1;
  var_t *v_parent = var_p;
  while (deref && var_p != NULL) {
    switch (code_peek()) {
    case kwTYPE_LEVEL_BEGIN:
      if (var_p->type == V_PTR) {
        *is_ptr = 1;
        deref = 0;
      } else {
        var_p = code_get_map_element(v_parent, var_p);
      }
      break;
    case kwTYPE_UDS_EL:
      var_p = map_resolve_fields(var_p, NULL);
      break;
    default:
      deref = 0;
    }
    var_p = eval_ref_var(var_p);
  }
  return var_p;
}

/**
 * returns true if the next code is a variable. if the following code is an
 * expression (no matter if the first item is a variable), returns false
 */
int code_isvar() {
  if (code_peek() == kwTYPE_VAR) {
    int is_ptr;
    var_t *basevar_p;
    bcip_t cur_ip = prog_ip;

    code_skipnext();
    var_t *var_p = basevar_p = tvar[code_getaddr()];
    switch (basevar_p->type) {
    case V_MAP:
      is_ptr = 0;
      var_p = code_isvar_resolve_map(var_p, &is_ptr);
      if (is_ptr) {
        // restore IP
        prog_ip = cur_ip;
        return 1;
      }
      break;
    case V_ARRAY:
      var_p = code_resolve_varptr(var_p, 0);
      break;
    case V_REF:
      is_ptr = 0;
      var_p = resolve_var_ref(var_p, &is_ptr);
      if (is_ptr) {
        prog_ip = cur_ip;
        return 1;
      }
      break;
    default:
      if (code_peek() == kwTYPE_LEVEL_BEGIN) {
        var_p = NULL;
      }
    }
    if (var_p) {
      byte code = code_peek();
      if (code == kwTYPE_EOC ||
          code == kwTYPE_SEP ||
          code == kwTYPE_LEVEL_END ||
          kw_check_evexit(code)) {
        // restore IP
        prog_ip = cur_ip;
        return 1;
      }
    }

    // restore IP
    prog_ip = cur_ip;
  }
  return 0;
}

var_t *eval_ref_var(var_t *var_p) {
  var_t *result = var_p;
  while (result != NULL && result->type == V_REF) {
    if (result->v.ref == var_p) {
      // circular referance error
      result = NULL;
      err_ref_circ_var();
      break;
    } else {
      result = result->v.ref;
    }
  }
  return result;
}

/*
 * evaluate the pcode string
 */
void v_eval_str(var_p_t v) {
  int len = code_getstrlen();
  v->type = V_STR;
  v->v.p.length = len;
  v->v.p.ptr = (char *)&prog_source[prog_ip];
  v->v.p.owner = 0;
  prog_ip += len;
}

