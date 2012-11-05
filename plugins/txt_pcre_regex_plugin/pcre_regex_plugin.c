/***************************************************************************
*
*   File    : pcre_regex_plugin.c
*   Purpose : Implements a pcre_regex plugin with PCRE functionalities
*
*   Author: David Ruano, José Ramón Méndez
*
*
*   Date    : December  9, 2010
*
*****************************************************************************
*   LICENSING
*****************************************************************************
*
* WB4Spam: An ANSI C is an open source, highly extensible, high performance and
* multithread spam filtering platform. It takes concepts from SpamAssassin project
* improving distinct issues.
*
* Copyright (C) 2010, by Sing Research Group (http://sing.ei.uvigo.es)
*
* This file is part of WireBrush for Spam project.
*
* Wirebrush for Spam is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public License as
* published by the Free Software Foundation; either version 3 of the
* License, or (at your option) any later version.
*
* Wirebrush for Spam is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
* General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
***********************************************************************/
#include <stdlib.h>
#include <string.h>
#include <cpluff.h>
#include <regex.h>
#include <stdio.h>
#include "core.h"
#include "hashmap.h"
#include "pcre_regex_util.h"
#include "logger.h"
#include "parse_func_args.h"
#include "eml_parser.h"

struct pcre_regex_data{
      map_t pcre_regex_cache;
      function_t *funcs;
      cp_context_t *ctx;
};

typedef struct pcre_regex_data pcre_regex_data;

/**
 * Free a regex on the map
 */
int free_pcre_regex(void *nullpointer, void *pcre_regex, void *key){
    pcregex_free(pcre_regex);
    free(key);
    return MAP_OK;
}

static int pcre_eval(void *_data, void *content, char *params, const char *flags){//, int parserType){
    //el msq es un char * ya que recibe el mail en modo raw.
    //Recibe un solo parametro que es el pattern
        
    char *target_content=(char *)content;
    pcre *pcre_regex;
    char *expr, *tmp;
    pcre_regex_data *data=(pcre_regex_data *)_data;    
    function_arguments *arguments=parse_args(params,1);

    if(get_argument_type(arguments,0)!=TYPE_STRING){
    	wblprintf(LOG_WARNING,"PCRE REGEX PLUGIN(pcre_eval)","Incorrect arguments %s\n", params);
    	return 0;
    }
    
    expr=get_argument_content(arguments,0);
    
    if(data==NULL || hashmap_get(data->pcre_regex_cache,expr,(any_t *)&pcre_regex)==MAP_MISSING){
        pcre_regex=pcregex_compile(expr);
        if (pcre_regex==NULL) {
             wblprintf(LOG_WARNING,"PCRE REGEX PLUGIN(pcre_eval)","Incorrect regular expression %s\n", params);
             return 0;
        }
        if (data!=NULL){
            tmp=malloc(sizeof(char)*(strlen(expr)+1)); //Make a copy of the uncompiled regex for cache
            //printf("KEY INSERTADA%s\n",tmp);
            strcpy(tmp,expr);
            hashmap_put(data->pcre_regex_cache,tmp,(any_t)pcre_regex);
        }
    }
    
    free_arguments(arguments);
    
    return pcregex_match(pcre_regex,target_content);
}

/**
 * Create a plug-in instance.
 */
static void *create(cp_context_t *ctx){
    pcre_regex_data *data;
    data=malloc(sizeof(pcre_regex_data));
    data->ctx=ctx;    
    
    data->pcre_regex_cache = hashmap_new();

    data->funcs=malloc(sizeof(function_t));
    data->funcs->function=&pcre_eval;
    data->funcs->conf_function=NULL;
    data->funcs->data=data;
    
    return data;
}

/**
 * Initializes and starts the plug-in.
 */
static int start(void *d) {
    pcre_regex_data *data=(pcre_regex_data *)d;

    //Dinamyc plugin initialization
    if (cp_define_symbol(data->ctx, "es_uvigo_ei_pcre_eval", data->funcs)==CP_OK)
       return CP_OK;
    else
       return CP_ERR_RESOURCE;
}

/**
 * Release resources
 */
static void stop(void *d) {
    //pcre_regex_data *data=(struct pcre_regex_data *)d;
    
    //Free all compiled regex
    //hashmap_iterate_elements(data->pcre_regex_cache,&free_pcre_regex,NULL);
    //hashmap_iterate_keys(data->pcre_regex_cache,&free_key,NULL);
    //free the hashmaps
    //hashmap_free(data->pcre_regex_cache);
}

/**
 * Destroys a plug-in instance.
 */
static void destroy(void *d) {
    pcre_regex_data *data=(struct pcre_regex_data *)d;

    hashmap_iterate_elements(data->pcre_regex_cache,&free_pcre_regex,NULL);
    //hashmap_iterate_keys(data->pcre_regex_cache,&free_key,NULL);
    //free the hashmaps
    hashmap_free(data->pcre_regex_cache);
    
    free(data->funcs);

    free(data);
}

/* ------------------------------------------------------------------------
 * Exported classifier information
 * ----------------------------------------------------------------------*/

CP_EXPORT cp_plugin_runtime_t pcre_regex_plugin_runtime_functions = {create, start, stop, destroy};
