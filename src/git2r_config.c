/*
 *  git2r, R bindings to the libgit2 library.
 *  Copyright (C) 2013-2017 The git2r contributors
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License, version 2,
 *  as published by the Free Software Foundation.
 *
 *  git2r is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <Rdefines.h>
#include "git2.h"

#include "git2r_arg.h"
#include "git2r_error.h"
#include "git2r_repository.h"

#define GIT2R_N_CONFIG_LEVELS 7

/**
 * Count number of config variables by level
 *
 * @param cfg where to count the variables
 * @param n_level array to store the number of variables
 * @param 0 on succes, or error code
 */
static int git2r_config_count_variables(
    const git_config *cfg,
    size_t *n_level)
{
    int err;
    git_config_iterator *iterator = NULL;

    err = git_config_iterator_new(&iterator, cfg);
    if (err)
        return err;

    for (;;) {
        git_config_entry *entry;
        err = git_config_next(&entry, iterator);
        if (err) {
            if (GIT_ITEROVER == err)
                err = GIT_OK;
            goto cleanup;
        }

        switch (entry->level) {
        case GIT_CONFIG_LEVEL_PROGRAMDATA:
            n_level[0]++;
            break;
        case GIT_CONFIG_LEVEL_SYSTEM:
            n_level[1]++;
            break;
        case GIT_CONFIG_LEVEL_XDG:
            n_level[2]++;
            break;
        case GIT_CONFIG_LEVEL_GLOBAL:
            n_level[3]++;
            break;
        case GIT_CONFIG_LEVEL_LOCAL:
            n_level[4]++;
            break;
        case GIT_CONFIG_LEVEL_APP:
            n_level[5]++;
            break;
        case GIT_CONFIG_HIGHEST_LEVEL:
            n_level[6]++;
            break;
        default:
            giterr_set_str(GITERR_CONFIG,
                           git2r_err_unexpected_config_level);
            err = GIT_ERROR;
            goto cleanup;
        }
    }

cleanup:
    if (iterator)
        git_config_iterator_free(iterator);

    return err;
}

/**
 * Intialize a list for a config level. The list is only created if
 * there are any entries at that level.
 *
 * @param level the index of the level
 * @param n_level vector with number of entries per level
 * @param name name of the level to initialize
 * @return index of the config level list in the owning list
 */
static size_t git2r_config_list_init(
    SEXP list,
    size_t level,
    size_t *n_level,
    size_t *i_list,
    size_t i,
    const char *name)
{
    if (n_level[level]) {
        SEXP item;
        SEXP names;

        i_list[level] = i++;
        SET_VECTOR_ELT(
            list,
            i_list[level],
            item = allocVector(VECSXP, n_level[level]));
        setAttrib(item, R_NamesSymbol, allocVector(STRSXP, n_level[level]));
        names = getAttrib(list, R_NamesSymbol);
        SET_STRING_ELT(names, i_list[level] , mkChar(name));
    }

    return i;
}

/**
 * Add entry to result list.
 *
 * @param list the result list
 * @param level the level of the entry
 * @param i_level vector with the index where to add the entry within
 * the level
 * @param i_list vector with the index to the sub-list of the list at
 * level
 * @param entry the config entry to add
 * @return void
 */
static void git2r_config_list_add_entry(
    SEXP list,
    size_t level,
    size_t *i_level,
    size_t *i_list,
    git_config_entry *entry)
{
    if (i_list[level] < (size_t)LENGTH(list)) {
        SEXP sub_list = VECTOR_ELT(list, i_list[level]);

        if (i_level[level] < (size_t)LENGTH(sub_list)) {
            SEXP names = getAttrib(sub_list, R_NamesSymbol);
            SET_STRING_ELT(names, i_level[level], mkChar(entry->name));
            SET_VECTOR_ELT(sub_list, i_level[level], mkString(entry->value));
            i_level[level]++;
            return;
        }
    }
}

/**
 * List config variables
 *
 * @param cfg Memory representation the configuration file for this
 * repository.
 * @param list The result list
 * @param n_level vector with number of entries per level
 * @return 0 if OK, else error code
 */
static int git2r_config_list_variables(
    git_config *cfg,
    SEXP list,
    size_t *n_level)
{
    int err;
    size_t i_level[GIT2R_N_CONFIG_LEVELS] = {0}; /* Current index at level */
    size_t i_list[GIT2R_N_CONFIG_LEVELS] = {0};  /* Index of level in target list */
    git_config_iterator *iterator = NULL;
    size_t i = 0;

    err = git_config_iterator_new(&iterator, cfg);
    if (err)
        goto cleanup;

    i = git2r_config_list_init(list, 0, n_level, i_list, i, "programdata");
    i = git2r_config_list_init(list, 1, n_level, i_list, i, "system");
    i = git2r_config_list_init(list, 2, n_level, i_list, i, "xdg");
    i = git2r_config_list_init(list, 3, n_level, i_list, i, "global");
    i = git2r_config_list_init(list, 4, n_level, i_list, i, "local");
    i = git2r_config_list_init(list, 5, n_level, i_list, i, "app");
    i = git2r_config_list_init(list, 6, n_level, i_list, i, "highest");

    for (;;) {
        git_config_entry *entry;
        err = git_config_next(&entry, iterator);
        if (err) {
            if (GIT_ITEROVER == err)
                err = GIT_OK;
            goto cleanup;
        }

        switch (entry->level) {
        case GIT_CONFIG_LEVEL_PROGRAMDATA:
            git2r_config_list_add_entry(list, 0, i_level, i_list, entry);
            break;
        case GIT_CONFIG_LEVEL_SYSTEM:
            git2r_config_list_add_entry(list, 1, i_level, i_list, entry);
            break;
        case GIT_CONFIG_LEVEL_XDG:
            git2r_config_list_add_entry(list, 2, i_level, i_list, entry);
            break;
        case GIT_CONFIG_LEVEL_GLOBAL:
            git2r_config_list_add_entry(list, 3, i_level, i_list, entry);
            break;
        case GIT_CONFIG_LEVEL_LOCAL:
            git2r_config_list_add_entry(list, 4, i_level, i_list, entry);
            break;
        case GIT_CONFIG_LEVEL_APP:
            git2r_config_list_add_entry(list, 5, i_level, i_list, entry);
            break;
        case GIT_CONFIG_HIGHEST_LEVEL:
            git2r_config_list_add_entry(list, 6, i_level, i_list, entry);
            break;
        default:
            giterr_set_str(GITERR_CONFIG,
                           git2r_err_unexpected_config_level);
            err = GIT_ERROR;
            goto cleanup;
        }
    }

cleanup:
    if (iterator)
        git_config_iterator_free(iterator);

    return err;
}

/**
 * Open configuration file
 *
 * @param out Pointer to store the loaded configuration.
 * @param repo S4 class git_repository. If non-R_NilValue open the
 * configuration file for the repository. If R_NilValue open the
 * global, XDG and system configuration files.
 * @param snapshot Open a snapshot of the configuration.
 * @return 0 on success, or an error code.
 */
static int git2r_config_open(git_config **out, SEXP repo, int snapshot)
{
    int err;

    if (repo != R_NilValue) {
        git_repository *repository = git2r_repository_open(repo);
        if (!repository)
            git2r_error(__func__, NULL, git2r_err_invalid_repository, NULL);

        if (snapshot)
            err = git_repository_config_snapshot(out, repository);
        else
            err = git_repository_config(out, repository);

        git_repository_free(repository);
    } else if (snapshot) {
        git_config *config = NULL;

        err = git_config_open_default(&config);
        if (err) {
            git_config_free(config);
            return err;
        }

        err = git_config_snapshot(out, config);

        git_config_free(config);
    } else {
        err = git_config_open_default(out);
    }

    return err;
}

/**
 * Get config variables
 *
 * @param repo S4 class git_repository
 * @return VECSXP list with variables by level
 */
SEXP git2r_config_get(SEXP repo)
{
    int err;
    SEXP result = R_NilValue;
    size_t i = 0, n = 0, n_level[GIT2R_N_CONFIG_LEVELS] = {0};
    git_config *cfg = NULL;

    err = git2r_config_open(&cfg, repo, 0);
    if (err)
        goto cleanup;

    err = git2r_config_count_variables(cfg, n_level);
    if (err)
        goto cleanup;

    /* Count levels with entries */
    for (; i < GIT2R_N_CONFIG_LEVELS; i++) {
        if (n_level[i])
            n++;
    }

    PROTECT(result = allocVector(VECSXP, n));
    setAttrib(result, R_NamesSymbol, allocVector(STRSXP, n));

    if (git2r_config_list_variables(cfg, result, n_level))
        goto cleanup;

cleanup:
    if (cfg)
        git_config_free(cfg);

    if (R_NilValue != result)
        UNPROTECT(1);

    if (err)
        git2r_error(__func__, giterr_last(), NULL, NULL);

    return result;
}

/**
 * Set or delete config entries
 *
 * @param repo S4 class git_repository
 * @param variables list of variables. If variable is NULL, it's deleted.
 * @return R_NilValue
 */
SEXP git2r_config_set(SEXP repo, SEXP variables)
{
    int err = 0, nprotect = 0;
    SEXP names;
    size_t i, n;
    git_config *cfg = NULL;

    if (git2r_arg_check_list(variables))
        git2r_error(__func__, NULL, "'variables'", git2r_err_list_arg);

    n = length(variables);
    if (n) {
        err = git2r_config_open(&cfg, repo, 0);
        if (err)
            goto cleanup;

        PROTECT(names = getAttrib(variables, R_NamesSymbol));
        nprotect++;
        for (i = 0; i < n; i++) {
            const char *key = CHAR(STRING_ELT(names, i));
            const char *value = NULL;

            if (!isNull(VECTOR_ELT(variables, i)))
                value = CHAR(STRING_ELT(VECTOR_ELT(variables, i), 0));

            if (value)
                err = git_config_set_string(cfg, key, value);
            else
                err = git_config_delete_entry(cfg, key);

            if (err) {
                if (err == GIT_EINVALIDSPEC) {
                    Rf_warning("Variable was not in a valid format: '%s'", key);
                    err = 0;
                } else {
                    goto cleanup;
                }
            }
        }

    }

cleanup:
    if (cfg)
        git_config_free(cfg);

    if (nprotect)
        UNPROTECT(nprotect);

    if (err)
        git2r_error(__func__, giterr_last(), NULL, NULL);

    return R_NilValue;
}

/**
 * Get the value of a string config variable
 *
 * @param repo S4 class git_repository
 * @param name The name of the variable
 * @return If the variable exists, a character vector of length one
 * with the value, else R_NilValue.
 */
SEXP git2r_config_get_string(SEXP repo, SEXP name)
{
    int err;
    SEXP result = R_NilValue;
    const char *value;
    git_config *cfg = NULL;

    if (git2r_arg_check_string(name))
        git2r_error(__func__, NULL, "'name'", git2r_err_string_arg);

    err = git2r_config_open(&cfg, repo, 1);
    if (err)
        goto cleanup;

    err = git_config_get_string(&value, cfg, CHAR(STRING_ELT(name, 0)));
    if (err) {
        if (err == GIT_ENOTFOUND)
            err = 0;
        goto cleanup;
    }

    PROTECT(result = allocVector(STRSXP, 1));
    SET_STRING_ELT(result, 0, mkChar(value));

cleanup:
    if (cfg)
        git_config_free(cfg);

    if (R_NilValue != result)
        UNPROTECT(1);

    if (err)
        git2r_error(__func__, giterr_last(), NULL, NULL);

    return result;
}

/**
 * Get the value of a boolean config variable
 *
 * @param repo S4 class git_repository
 * @param name The name of the variable
 * @return If the variable exists, a logical vector of length one
 * with TRUE or FALSE, else R_NilValue.
 */
SEXP git2r_config_get_logical(SEXP repo, SEXP name)
{
    int err;
    SEXP result = R_NilValue;
    int value;
    git_config *cfg = NULL;

    if (git2r_arg_check_string(name))
        git2r_error(__func__, NULL, "'name'", git2r_err_string_arg);

    err = git2r_config_open(&cfg, repo, 1);
    if (err)
        goto cleanup;

    err = git_config_get_bool(&value, cfg, CHAR(STRING_ELT(name, 0)));
    if (err) {
        if (err == GIT_ENOTFOUND)
            err = 0;
        goto cleanup;
    }

    PROTECT(result = allocVector(LGLSXP, 1));
    if (value)
        LOGICAL(result)[0] = 1;
    else
        LOGICAL(result)[0] = 0;

cleanup:
    if (cfg)
        git_config_free(cfg);

    if (R_NilValue != result)
        UNPROTECT(1);

    if (err)
        git2r_error(__func__, giterr_last(), NULL, NULL);

    return result;
}
