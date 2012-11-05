/***************************************************************************                        
*
*   File    : core.h
*   Purpose : Implements the core filtering system for WB4Spam
*            
*            
*   Author: David Ruano
* 
 * 
* 
*   Date  : April 13, 2011
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
#include <stdio.h>
#include <cpluff.h>
#include <ctype.h>
#include "core.h"
#include "hashmap.h"
#include <string.h>
#include "ruleset.h"
#include <pthread.h>
#include "string_util.h"
#include "logger.h"
#include "wb_system.h"
#include "eml_domain_parser.h"
#include "meta.h"
#include "schedule.h"
#include "vector.h"

//Evaluates rules only while the score is less than the required score.
#define NONE -2.0
#define ACTIVATED -1.0
#define SMART 2
#define NO_SPAM 3
#define SPAM 4

//Define learning method.
#define MANUAL_HAM 0
#define MANUAL_SPAM 1
#define AUTOLEARN -1
#define DISABLE_LEARN -2


#define RULE_MATCH 0
#define RULE_NOT_MATCH -1

//#define HAM_VALUE "False"
//#define SPAM_VALUE "True"

//Use multithreaded approach
#define DEFAULT_MULTITHREAD 0
#define DEFAULT_SCHEDULE_PLAN "default_scheduling"

#define EMAIL_CONTENT argv[0]
#define EMAIL_REPORT argv[1]
#define EMAIL_SCORES argv[2]
#define LEARN_METHOD argv[3]
#define PROGRAM_OPTION argv[4]

/* ------------------------------------------------------------------------
 * Data types
 * ----------------------------------------------------------------------*/

/** Type for plugin_data_t structure */
typedef struct plugin_data_t plugin_data_t;

/** Type for registered_function_t structure */
typedef struct registered_function_t registered_function_t;

/** Type for registered_parser_t structure */
typedef struct registered_parser_t registered_parser_t;

/** Type for registered_eventhandler_t structure */
typedef struct registered_eventhandler_t registered_eventhandler_t;

/** Type for registered_prescheduler_t structure */
typedef struct registered_prescheduler_t registered_prescheduler_t;

//LAZY EVALUATION DEFAULT VALUES
double LAZY_EVALUATION = NONE;
int ACTIVATED_EVALUATION = NONE;
int DEFINITIVE_RULE = RULE_NOT_MATCH;
int STOP=NONE;

short LEARN_TYPE=AUTOLEARN;
short PROGRAM_MODE=UNDEF_FILTER;

//MULTITHREAD DEFAULT VALUE
//short MULTITHREAD = DEFAULT_MULTITHREAD;

/** Plug-in instance data */
struct plugin_data_t {
	
    /** The plug-in context */
    cp_context_t *ctx;

    /** Number of registered functions */
    int num_functions;

    /** An array of registered functions */
    registered_function_t *functions;

    /** Number of registered parsers */
    int num_parsers;

    /** An array of registered parsers */
    registered_parser_t *parsers;

    /**A hashmap with registered parsers*/
    map_t parsers_map;

    /** Number of registered event handlers */
    int num_eventhandlers;

    /** An array of registered event handlers */
    registered_eventhandler_t *eventhandlers;

    /** A set of rules*/
    ruleset *rules;

    /** The score of an email*/
    float score;

    /** Hashmap for the executed rules**/
    map_t executed_rules;

    //Rules that match for an email
    char *stored_rules;
    
    /** A mutex for access the mutex **/
    pthread_mutex_t mutex4score;

    ini_file *config_values;

    //The positive maximun score.
    float positive_score;

    //The negative maximun score.
    float negative_score;

    //The to domain fiel for multi-domain rules.
    map_t to_domain;
    
    registered_prescheduler_t *preschedulers;
    
    int num_preschedulers;
    
    //prescheduler_func_t selected_preschedule;
    
    map_t running_eventhandlers;
    
    map_t parsedcontents;
    
    map_t preschedulers_map;
};

/** Registered function information */
struct registered_function_t {
	
    /** Config */
    char *cfg;

    /** Function name*/
    char * name;
    
    /** Function plugin*/
    char *plugin;

    /** The function data */
    function_t *function;
};

/** Registered parser information */
struct registered_parser_t {

    // Config
    char *cfg;
        
    //Parser name
    char *name;

    // The parser data
    parser_t *parser;
};

/** Registered event handler information */
struct registered_eventhandler_t {

    /** Config */
    char *cfg;
        
    /** Event handler name*/
    char *name;
    
    /** Function plugin*/
    char *plugin;    

    /** The parser data */
    eventhandler_t *function;
};

/** Registered prescheduler information */
struct registered_prescheduler_t {

    /** Config */
    char *cfg;
        
    /** Event handler name*/
    char *name;

    /** The parser data */
    prescheduler_t *function;
};

/**
 * Thread data structure for interchange information from threads
 */
struct threaddata{

    function_t *fp;

    char *name;
    char *tflags;
    char *params;
    char **stored_rules;    

    void *content;
    void *score;

    float *target_score;
    float *positive_score;
    float *negative_score;
    
    short rule_type;
    //float required_score;
    
    map_t executed_rules;

    pthread_mutex_t mutex4score;
};
typedef struct threaddata threaddata;

struct thread_metadata{
    char *name;
    char *expresion;
    char **stored_rules;
    
    void *score;
    
    float *target_score;
    float *positive_score;
    float *negative_score;
    
    short rule_type;
    
    map_t executed_rules;
    
    vector *dependant_rules;

    pthread_mutex_t mutex4score;
};
typedef struct thread_metadata thread_metadata;


/*
struct thread_def_data{

    //The function to call and the internal data
    function_t *fp;

    char *name;
    char *tflags;
    char *params;
    char **stored_rules;

    void *content;
    void *score;

    //float required_score;
    float *target_score;

    pthread_mutex_t mutex4score;
};
typedef struct thread_def_data thread_def_data;
*/


struct thread_eventhandler{
    eventhandler_t *eventhandler;
    
    void *parsed_content;
    
    short learn_type;
};

typedef struct thread_eventhandler thread_eventhandler;


/* ------------------------------------------------------------------------
 * Internal functions
 * ----------------------------------------------------------------------*/

int check_domains(map_t eml_domains,map_t ruleset_domains);

/**
 * Data liberation handler for hashmap_iterate
 * Is is able to free reserved memory to all parsers data.
 */
int free_parser_data(any_t parsers_map, any_t data, any_t key);
/**
 * Thread wait handler for hashmap_iterate
 * Is able to wait for any thread stored in a hashmap
 */
int pthread_join_data(any_t nullpointer, any_t data);


/**
 * Function handler for multiprocessing support
 */
/*
void *thread_callback(void *arg){
    threaddata *data=arg;
    float localscore;
    int *function_result=malloc(sizeof(int));
    
    
    *function_result=data->fp->function(data->fp->data,data->content,data->params,data->tflags);//,data->parserType);
    
    localscore=((*function_result)?data->score:0);

    //Update the global score
    pthread_mutex_lock(&(data->mutex4score));
    *(data->target_score)+=localscore;
    hashmap_put(data->executed_rules,data->name,function_result);
    if (*function_result){

        if(data->score > 0) *(data->positive_score)-=data->score;
        else *(data->negative_score)-=data->score;

        if(*(data->stored_rules)==NULL){
            *(data->stored_rules)=malloc(sizeof(char)*(strlen(data->name)+1));
            strcpy(*(data->stored_rules),data->name);
         }else{
            char * temp = malloc(sizeof(char)*strlen(data->name)+2);
            sprintf(temp,"|%s",data->name);
            *(data->stored_rules)=appendstr(*(data->stored_rules),temp);
            free(temp);
         }
    }

    pthread_mutex_unlock(&(data->mutex4score));
    
    free(arg);
    
    pthread_exit(NULL);
    
    return NULL;
}
*/


void *thread_callback(void *arg){
    threaddata *data=arg;
    int *function_result=malloc(sizeof(int));
    float localscore=0;
    
    *function_result=data->fp->function(data->fp->data,data->content,data->params,data->tflags);//,data->parserType);
    
    if (*function_result){
        
        pthread_mutex_lock(&(data->mutex4score));
        
        if(data->rule_type & DEFINITIVE_SCORE){

            DEFINITIVE_RULE=RULE_MATCH;
            printf("con score definitivo\n");
            //(strcmp((char *)data->score,"+")==0)?
            //    (*(data->target_score)=data->required_score):
            //    (*(data->target_score)=-data->required_score);

            if(*(data->stored_rules)==NULL){
                *(data->stored_rules)=malloc(sizeof(char)*(strlen(data->name)+1));
                sprintf(*(data->stored_rules),"%s",data->name);
            }
            else{
                char * temp = malloc(sizeof(char)*(strlen(data->name)+2));
                sprintf(temp,"|%s",data->name);
                *(data->stored_rules)=appendstr(*(data->stored_rules),temp);
                free(temp);
            }
        }else{
            localscore=data->score;
            printf("con score normal\n");
            //Update the global score
            pthread_mutex_lock(&(data->mutex4score));
            *(data->target_score)+=localscore;
            hashmap_put(data->executed_rules,data->name,function_result);
            if (*function_result){

                if(data->score > 0) *(data->positive_score)-=data->score;
                else *(data->negative_score)-=data->score;

                if(*(data->stored_rules)==NULL){
                    *(data->stored_rules)=malloc(sizeof(char)*(strlen(data->name)+1));
                    strcpy(*(data->stored_rules),data->name);
                 }else{
                    char * temp = malloc(sizeof(char)*strlen(data->name)+2);
                    sprintf(temp,"|%s",data->name);
                    *(data->stored_rules)=appendstr(*(data->stored_rules),temp);
                    free(temp);
                 }
            }
        }
        pthread_mutex_unlock(&(data->mutex4score));
    }
    
    free(arg);
    
    free(function_result);
    
    pthread_exit(NULL);
    
    return NULL;
}

void *thread_meta_callback(void *arg){
    thread_metadata *data=arg;
    float localscore=0;
    int *function_result=malloc(sizeof(int));
    //META-> DESCOMENTAR LA FUNCION
    //*function_result=evaluate_meta(data->expresion, data->executed_rules)>0;
    *function_result=0;
    printf("META: FUNCION COMENTADA->DEFAULT_VALUE=0\n");
    if (*function_result){
        
        pthread_mutex_lock(&(data->mutex4score));
    
        if(data->rule_type & DEFINITIVE_SCORE){

            DEFINITIVE_RULE=RULE_MATCH;

            if(*(data->stored_rules)==NULL){
                *(data->stored_rules)=malloc(sizeof(char)*(strlen(data->name)+1));
                sprintf(*(data->stored_rules),"%s",data->name);
            }
            else{
                char * temp = malloc(sizeof(char)*(strlen(data->name)+2));
                sprintf(temp,"|%s",data->name);
                *(data->stored_rules)=appendstr(*(data->stored_rules),temp);
                free(temp);
            }
        }
        else{

            localscore=data->score;

            *(data->target_score)+=localscore;
            hashmap_put(data->executed_rules,data->name,function_result);

            (data->score > 0)?(*(data->positive_score)-=data->score):(*(data->negative_score)-=data->score);

            if(*(data->stored_rules)==NULL){
                *(data->stored_rules)=malloc(sizeof(char)*(strlen(data->name)+1));
                strcpy(*(data->stored_rules),data->name);
             }else{
                char * temp = malloc(sizeof(char)*strlen(data->name)+2);
                sprintf(temp,"|%s",data->name);
                *(data->stored_rules)=appendstr(*(data->stored_rules),temp);
                free(temp);
             }
        }
        
        pthread_mutex_unlock(&(data->mutex4score));
    }
    
    free(function_result);
    
    free(arg);
    
    pthread_exit(NULL);
    
    return NULL;
}


void *thread_eventhandler_callback(void *arg){
    thread_eventhandler *data=(thread_eventhandler *)arg;

    data->eventhandler->function(data->eventhandler->data,data->parsed_content,data->learn_type);

    pthread_exit(NULL);
}

/**
 * A run function for the core plug-in. In this case this function acts as
 * the application main function so there is no need for us to split the
 * execution into small steps. Rather, we execute the whole main loop at
 * once to make it simpler.
 */
static int run(void *d) {
    plugin_data_t *data = d;
    //map_t parsedcontents=hashmap_new(); //Stores the parsed data from parsers
    char **argv;
    int argc;
    int j=0;
    void *parsedcontent;
    double cpu_load[3];
    
    hashmap_iterate(data->running_eventhandlers,&pthread_join_data,NULL);
    hashmap_free(data->running_eventhandlers);
    data->running_eventhandlers=hashmap_new();
    
    hashmap_iterate_elements(data->parsedcontents,&free_parser_data,data->parsers_map);
    hashmap_free(data->parsedcontents);
    data->parsedcontents=hashmap_new();
    //Liberates de hashmap.
        
    //Variables used for the LAZY EVALUATION.
    data->positive_score=get_positive_score(data->rules);
    data->negative_score=get_negative_score(data->rules);

    //Total score of the email.
    data->score=0;
    //Rules that match for a email.
    data->stored_rules=NULL;

    //Rules executed at the moment.
    data->executed_rules=hashmap_new();

    // Read arguments and print usage, if no arguments given
    argv = cp_get_context_args(data->ctx, &argc);
    

    // Execute functions

    threaddata *tdata=NULL;
    pthread_t *t;
    //pthread_attr_t tattr;

    pthread_mutex_init(&(data->mutex4score), NULL);

    /** The list of running plugins */
    map_t runningplugins=hashmap_new(); 
                          //for each plugin that is currently running we store a pthread_t pointer
                          //this pointer will be used for syncrhonizing purposes and to avoid
                          //concurrent executions of functions of the same plugin.

    //Initialize running plugins list
    //runningplugins=hashmap_new();

    wblprintf(LOG_DEBUG,"CORE","Multithread - Before processing rules...\n");

    while(j<count_rules(data->rules) && DEFINITIVE_RULE!=RULE_MATCH){
        short type=get_rule_characteristic(data->rules,j);
        if( type & NORMAL_RULE) {
                //PROCESING NORMAL RULES
                printf("Aqui van las reglas normales");
                if (hashmap_get(data->parsedcontents, get_parser_name(data->rules,j),
                                &parsedcontent)==MAP_MISSING){
                    parser_t *pp;
                    if( (pp = get_parser_pointer(data->rules,j))!=NULL ){
                        if ( (parsedcontent=pp->function(pp->data,EMAIL_CONTENT)) ==NULL)
                            wblprintf(LOG_WARNING,"CORE","Parsed_content is null\n");
                        else{
                            char *to_field=NULL;
                            hashmap_put(data->parsedcontents, get_parser_name(data->rules,j), parsedcontent);

                            if(PROGRAM_MODE==SPAM_FILTER){
                                if ( (to_field=get_eml_to_field((map_t)parsedcontent))!=NULL){
                                    (data->to_domain==NULL)?
                                        (data->to_domain=get_eml_domains(to_field)):
                                        (0);
                                }else wblprintf(LOG_WARNING,"CORE","Unable to obtain email destination field\n");
                            }
                        }
                    }else{
                        wblprintf(LOG_WARNING,"CORE","Parser is null\n");
                        continue;
                    }

                }
                map_t ruledomain=get_rule_domain(data->rules,j);
                if(ruledomain==NULL || (ruledomain!=NULL && check_domains(data->to_domain,ruledomain)>0))
                {
                    function_t *fp = get_definition_pointer(data->rules,j);
                    if (fp!=NULL){
//                        if(type & DEFINITIVE_SCORE){
//                            printf("con score definitivo\n");
//                            //If function returns true then add the rule asociated score. Otherwise add 0
//                            tdata=malloc(sizeof(threaddata));
//                            tdata->fp=fp;
//                            tdata->content=parsedcontent;
//                            tdata->params=get_definition_param(data->rules,j);
//                            tdata->tflags=get_tflags(data->rules,j);
//                            tdata->target_score=&(data->score);
//                            tdata->mutex4score=data->mutex4score;
//                            tdata->stored_rules=&(data->stored_rules);
//                            tdata->rule_type=type;
                            //tdata->score=get_rule_score(data->rules,j);
//                            tdata->name=get_rulename(data->rules,j);
//                            if (hashmap_get(runningplugins,get_definition_plugin(data->rules,j),(any_t *)&t)!=MAP_MISSING){
//                                pthread_join(*t,NULL);
//                                pthread_detach(*t); //Detach the thread
//                                hashmap_remove(runningplugins,get_definition_plugin(data->rules,j));
//                            }else t=malloc(sizeof(pthread_t));

//                            pthread_create(t,NULL,&thread_callback,tdata);
//                            wblprintf(LOG_DEBUG,"CORE","Launching thread for plugin %s\n",get_definition_plugin(data->rules,j));

//                            hashmap_put(runningplugins,get_definition_plugin(data->rules,j),(any_t)t);
//                            wblprintf(LOG_DEBUG,"CORE","Thread started - %s...\n",get_rulename(data->rules,j));

//                        }else{
                            //printf("con score no definitivo\n");
                            tdata=malloc(sizeof(threaddata));
                            tdata->fp=fp;
                            tdata->content=parsedcontent;
                            tdata->params=get_definition_param(data->rules,j);
                            tdata->tflags=get_tflags(data->rules,j);
                            tdata->score=get_rule_score(data->rules,j);
                            tdata->target_score=&(data->score);
                            tdata->mutex4score=data->mutex4score;
                            tdata->stored_rules=&(data->stored_rules);
                            tdata->name=get_rulename(data->rules,j);
                            tdata->executed_rules=data->executed_rules;
                            tdata->positive_score=&(data->positive_score);
                            tdata->negative_score=&(data->negative_score);
                            tdata->rule_type=type;
                            if (hashmap_get(runningplugins,get_definition_plugin(data->rules,j),(any_t *)&t)!=MAP_MISSING){
                                pthread_join(*t,NULL);
                                pthread_detach(*t); //Detach the thread
                                hashmap_remove(runningplugins,get_definition_plugin(data->rules,j));
                            }else t=malloc(sizeof(pthread_t));

                            pthread_create(t,NULL,&thread_callback,tdata);
                            wblprintf(LOG_DEBUG,"CORE","Launching thread for plugin %s\n",get_definition_plugin(data->rules,j));
                            hashmap_put(runningplugins,get_definition_plugin(data->rules,j),(any_t)t);
                            wblprintf(LOG_DEBUG,"CORE","Thread started - %s...\n",get_rulename(data->rules,j));

/*
                            get_cpu_load(cpu_load);

                            if(LAZY_EVALUATION == ACTIVATED || 
                               LAZY_EVALUATION >= cpu_load[0]){
                                if( (data->score < get_required_score(data->rules)) &&
                                    (data->score + data->positive_score) < get_required_score(data->rules)){
                                    ACTIVATED_EVALUATION = NO_SPAM;
                                    break;
                                }
                                if( ((data->score > get_required_score(data->rules)) &&
                                     (data->score + data->negative_score) > get_required_score(data->rules)) ||
                                    ((data->score == get_required_score(data->rules)) &&
                                     ((data->negative_score==0) && (data->positive_score > 0))) ){
                                    ACTIVATED_EVALUATION = SPAM;
                                    break;
                                }
                            }  
*/
                        //}
                    }else wblprintf(LOG_WARNING,"CORE","Function is null\n");
                    
                }else{
                    wblprintf(LOG_INFO,"CORE","Domain for rule not found. Aborting '%s' rule execution\n",get_rulename(data->rules,j));
                    continue;
                }
        }
        else{
            //PROCESSING META RULES
            printf("Reglas meta ");
            map_t ruledomain=get_rule_domain(data->rules,j);            
            if(ruledomain==NULL || (ruledomain!=NULL && check_domains(data->to_domain,ruledomain)>0)){            
                //if(type & NORMAL_SCORE){
                //META RULES WITH NORMAL SCORE
                if(LAZY_EVALUATION!=NONE && get_meta_score(data->rules,j)!=0){
                    thread_metadata *mdata=malloc(sizeof(thread_metadata));
                    mdata->name=get_rulename(data->rules,j);
                    mdata->expresion=get_meta_definition(data->rules,j);
                    mdata->score=get_rule_score(data->rules,j);
                    mdata->target_score=&(data->score);
                    mdata->mutex4score=data->mutex4score;
                    mdata->stored_rules=&(data->stored_rules);
                    mdata->executed_rules=data->executed_rules;
                    mdata->positive_score=&(data->positive_score);
                    mdata->negative_score=&(data->negative_score);
                    mdata->dependant_rules=get_dependant_rules(data->rules,j);
                    mdata->rule_type=type;

                    t=malloc(sizeof(pthread_t));

                    wblprintf(LOG_DEBUG,"CORE","Launching thread for meta rule %s\n",mdata->name);                        
                    pthread_create(t,NULL,&thread_meta_callback,mdata);

                    pthread_join(*t, NULL);
                    pthread_detach(*t);
                    free(t);
                    free(mdata);
                } else wblprintf(LOG_INFO,"CORE","LAZY_EVALUATION. Rule score is 0. Aborting rule execution\n");

            }
            else{
                wblprintf(LOG_INFO,"CORE","Rule domain not found. Aborting '%s' rule execution\n",get_rulename(data->rules,j));
                (get_rule_score(data->rules,j)>0)?
                    (data->positive_score-=get_rule_score(data->rules,j)):
                    (data->negative_score-=get_rule_score(data->rules,j));
            }
        }
        if(DEFINITIVE_RULE==RULE_MATCH) break;
        
        get_cpu_load(cpu_load);

        if(LAZY_EVALUATION==ACTIVATED || LAZY_EVALUATION >= cpu_load[0]){
            if( (data->score < get_required_score(data->rules)) &&
               (data->score + data->positive_score) < get_required_score(data->rules)){
                ACTIVATED_EVALUATION = NO_SPAM;
                break;
            }
            if( (data->score > get_required_score(data->rules)) &&
               (data->score + data->negative_score) > get_required_score(data->rules)){
                ACTIVATED_EVALUATION = SPAM;
                break;
            }
            if( (data->score == get_required_score(data->rules)) &&
                ((data->negative_score==0) && (data->positive_score > 0))){
                ACTIVATED_EVALUATION = SPAM;
                break;
            }
        }
        
        j++;
    }

    if(DEFINITIVE_RULE==RULE_MATCH){
        wblprintf(LOG_INFO,"CORE","Definitive rule match. Aborting filter execution...\n");
        
        if (EMAIL_REPORT!=NULL) {
           free(EMAIL_REPORT);
           EMAIL_REPORT=NULL;
        }
        if (EMAIL_SCORES!=NULL){
           free(EMAIL_SCORES);
           EMAIL_SCORES=NULL;
        }

        if (data->stored_rules!=NULL){
          EMAIL_REPORT=malloc(sizeof(char)*(strlen(data->stored_rules)+7));
          EMAIL_SCORES=malloc((sizeof(char)*5)+(2*sizeof(float)));
          sprintf(EMAIL_REPORT,"(%s|...)",data->stored_rules);
          (data->score==data->rules->required)?
              (sprintf(EMAIL_SCORES,"%2.2f+/%2.2f",get_required_score(data->rules),get_required_score(data->rules))):
              (sprintf(EMAIL_SCORES,"%2.2f-/%2.2f",get_required_score(data->rules),get_required_score(data->rules)));
        }
        else{
          EMAIL_REPORT=malloc(sizeof(char)*3);
          EMAIL_SCORES=malloc((sizeof(char)*5)+(2*sizeof(float)));
          strcpy(EMAIL_REPORT,"()");
          (data->score==data->rules->required)?
              (sprintf(EMAIL_SCORES,"%2.2f+/%2.2f",get_required_score(data->rules),get_required_score(data->rules))):
              (sprintf(EMAIL_SCORES,"%2.2f-/%2.2f",get_required_score(data->rules),get_required_score(data->rules)));
        }
    
    }
    else{
        if (EMAIL_REPORT!=NULL){
           free(EMAIL_REPORT);
           EMAIL_REPORT=NULL;
        }
        
        if (EMAIL_SCORES!=NULL){
           free(EMAIL_SCORES);
           EMAIL_SCORES=NULL;
        }
        
        if(ACTIVATED_EVALUATION!=NONE){
        
            (ACTIVATED_EVALUATION==NO_SPAM)?
               (wblprintf(LOG_INFO,"CORE","SMART LAZY EVALUATION: No more rules needed to classify email as Legitimate\n")):
               (wblprintf(LOG_INFO,"CORE","SMART LAZY EVALUATION: No more rules needed to classify email as Spam\n"));

            if (data->stored_rules!=NULL){
              EMAIL_REPORT=malloc(sizeof(char)*(strlen(data->stored_rules)+7));
              EMAIL_SCORES=malloc((sizeof(char)*4)+(2*sizeof(float)));
              sprintf(EMAIL_REPORT,"(%s|...)",data->stored_rules);//,stored_rules);
              sprintf(EMAIL_SCORES,"%2.2f/%2.2f",data->score,get_required_score(data->rules));
              //(data->score<=get_required_score(data->rules))?(EMAIL_CLASS=SPAM_VALUE):(EMAIL_CLASS=HAM_VALUE);
            }
            else{
              EMAIL_REPORT=malloc(sizeof(char)*3);
              EMAIL_SCORES=malloc((sizeof(char)*3)+(2*sizeof(float)));
              strcpy(EMAIL_REPORT,"()");
              sprintf(EMAIL_SCORES,"%2.2f/%2.2f",data->score,get_required_score(data->rules));//,stored_rules);
              //(data->score<=get_required_score(data->rules))?(EMAIL_CLASS=SPAM_VALUE):(EMAIL_CLASS=HAM_VALUE);          
           }
        }
        else{
           if (data->stored_rules!=NULL){
              EMAIL_REPORT=malloc(sizeof(char)*(strlen(data->stored_rules)+3));
              EMAIL_SCORES=malloc((sizeof(char)*4)+(2*sizeof(float)));
              sprintf(EMAIL_REPORT,"(%s)",data->stored_rules);//,stored_rules);
              sprintf(EMAIL_SCORES,"%2.2f/%2.2f",data->score,get_required_score(data->rules));
           }else{
              int count=0;
              (data->score)?(count++):(0);
              (get_required_score(data->rules))?(count++):(0);
              EMAIL_REPORT=malloc(sizeof(char)*3);
              EMAIL_SCORES=malloc((sizeof(char)*(3+count))+(2*sizeof(float)));
              strcpy(EMAIL_REPORT,"()");
              sprintf(EMAIL_SCORES,"%2.2f/%2.2f",data->score,get_required_score(data->rules));//,stored_rules);          
           }
           // All done
        }
    }
    
    wblprintf(LOG_DEBUG,"CORE","Multicore - End rule launching\n");
    wblprintf(LOG_DEBUG,"CORE","Multicore - End rule join and removal\n");
    pthread_mutex_destroy (&(data->mutex4score));        
    wblprintf(LOG_DEBUG,"CORE","Multicore - Freeying hashmap\n");
    
    //Asegurar que todas las funciones han terminado
    hashmap_iterate(runningplugins, &pthread_join_data, NULL);
    hashmap_free(runningplugins);
    
    //---EVENTHANDLERS---
    if(LEARN_TYPE!=DISABLE_LEARN){
        (LEARN_TYPE==AUTOLEARN)?
            (LEARN_TYPE=(data->score>get_required_score(data->rules))):
            (0);

        void *parsed_content=NULL;
        registered_parser_t *pp=NULL;
        thread_eventhandler *handler_data;
        for(j=0;j<data->num_eventhandlers;j++){
            //printf("num_eventhandlers %d\n",data->num_eventhandlers);
            if(hashmap_get(data->parsedcontents,
                           data->eventhandlers[j].function->parser_name,
                           (any_t *)&parsed_content)==MAP_MISSING)
            {
                if(hashmap_get(data->parsers_map,
                               data->eventhandlers[j].function->parser_name,
                               (any_t *)&pp)==MAP_MISSING)
                {
                    wblprintf(LOG_WARNING,"CORE","Parser '%s' does not exist... Ignoring eventhandler\n",get_parser_name(data->rules,j));
                }
                else{
                    if(pp==NULL) 
                        wblprintf(LOG_CRITICAL,"CORE","Parser is null\n");
                    else{
                        parsed_content=pp->parser->function(pp->parser->data,EMAIL_CONTENT);
                        hashmap_put(data->parsedcontents,data->eventhandlers[j].function->parser_name,parsed_content);                        
                        
                        handler_data=malloc(sizeof(thread_eventhandler));
                        handler_data->eventhandler=data->eventhandlers[j].function;
                        handler_data->learn_type=LEARN_TYPE;
                        handler_data->parsed_content=parsed_content;
                        pthread_t *handler_thread=malloc(sizeof(pthread_t));
                        hashmap_put(data->running_eventhandlers,data->eventhandlers[j].name,handler_thread);
                        if(data->eventhandlers[j].function!=NULL)
                           pthread_create(handler_thread,NULL,&thread_eventhandler_callback,handler_data);
                        else wblprintf(LOG_CRITICAL,"CORE","Function eventhandler is null\n");
                    }
                }
            }
            else{
                handler_data=malloc(sizeof(thread_eventhandler));
                handler_data->eventhandler=data->eventhandlers[j].function;
                handler_data->learn_type=LEARN_TYPE;
                handler_data->parsed_content=parsed_content;

                pthread_t *handler_thread=malloc(sizeof(pthread_t));
                hashmap_put(data->running_eventhandlers,data->eventhandlers[j].name,handler_thread);
                (data->eventhandlers[j].function!=NULL)?
                    (pthread_create(handler_thread,NULL,&thread_eventhandler_callback,handler_data)):
                    (wblprintf(LOG_CRITICAL,"CORE","Function eventhandler is null\n"));
            }
        }
    }else wblprintf(LOG_INFO,"CORE","LEARN DISABLE: Eventhandler execution aborted\n");

    //More work to do (0 means no work remaining)
    return 1;
}

int verify_domains(any_t ruleset_domains,any_t occurrences, any_t key){
    any_t nullpointer;
    map_t r_domains=(map_t)ruleset_domains;
    int *num_occ=(int *)occurrences;
    
    if(hashmap_get(r_domains,(char *)key,(any_t *)&nullpointer)!=MAP_MISSING)
        *num_occ=*num_occ+1;
    return MAP_OK;
}

int check_domains(map_t eml_domains,map_t ruleset_domains){
    int *occurrences=malloc(sizeof(int));
    *occurrences=0;

    hashmap_iterate_three(eml_domains,&verify_domains,occurrences,ruleset_domains);
    int retval=*occurrences;
    free(occurrences);
    return retval;
}

/**
 * Creates a new plug-in instance.
 */
static void *create(cp_context_t *ctx) {
    plugin_data_t *data = malloc(sizeof(plugin_data_t));

    if (data != NULL) {
        data->ctx = ctx;
        data->num_functions = 0;
        data->num_parsers = 0;
        data->num_eventhandlers = 0;
        data->num_preschedulers = 0;
        data->eventhandlers = NULL;
        data->functions = NULL;
        data->parsers = NULL;
        data->parsers_map=NULL;
        data->stored_rules=NULL;
        data->score=0;
        data->executed_rules=NULL;
        data->config_values=NULL;
        data->positive_score=0;
        data->negative_score=0;
        data->to_domain=NULL;
        data->running_eventhandlers=hashmap_new();
        data->parsedcontents=hashmap_new();
        data->preschedulers=NULL;

    } else cp_log(ctx, CP_LOG_ERROR,"Insufficient memory for plug-in data.");

    //printf("Core: Create");
    return data;
}

/**
 * Initializes and starts the plug-in.
 */
static int start(void *d) {
    
    plugin_data_t *data = d;
    cp_extension_t **cl_exts;
    int num_cl_exts;
    cp_status_t status;
    int i;
    filelist *site_config_path_fl;
    char *value;
    char **argv;
    int argc;
    char *file_config_path=spam_file_config_path;
    char *rule_config_path=spam_rule_config_path;

    
    //if(has_attribute_value_ini(data->config_values,"CORE","multithread","ON"))
    //    MULTITHREAD=1;
    
    map_t parsers_map = hashmap_new();
    map_t functions_map = hashmap_new();
    map_t plugins_map = hashmap_new();
    
    argv = cp_get_context_args(data->ctx, &argc);
    PROGRAM_MODE=atoi(PROGRAM_OPTION);
    if(PROGRAM_MODE==CMS_FILTER){
        file_config_path=cms_file_config_path;
        rule_config_path=cms_rule_config_path;
        wblprintf(LOG_INFO,"CORE","Using CMS filtering platform\n");
    }else{
        file_config_path=spam_file_config_path;
        rule_config_path=spam_rule_config_path;
        wblprintf(LOG_INFO,"CORE","Using SPAM filtering platform\n");
    }
    
    parse_ini(file_config_path,&(data->config_values));

///-----------------------------------------------------------------------
    // Obtain list of registered functions
    cl_exts = cp_get_extensions_info(
            data->ctx,
            "es.uvigo.ei.core.functions",
            &status,
            &num_cl_exts
    );
    if (cl_exts == NULL) {
        wblprintf(LOG_CRITICAL,"CORE","Framework error on scanning function extensions..\n");
        // An error occurred and framework logged it
        //exit(-1);
        return status;
    }

    // Allocate memory for function information, if any
    if (num_cl_exts > 0) {
        data->functions = malloc(
                num_cl_exts * sizeof(registered_function_t)
        );
        if (data->functions == NULL) {
                // Memory allocation failed
                cp_log(data->ctx, CP_LOG_ERROR,
                        "Insufficient memory for function list");
                return CP_ERR_RESOURCE;
        }
    }

    //(MULTITHREAD)?
    //    (wblprintf(LOG_INFO,"CORE","MULTITHREAD ENABLED\n")):
    //    (wblprintf(LOG_INFO,"CORE","MULTITHREAD DISABLED\n"));


    /* Resolve function functions. This will implicitly start
     * plug-ins providing the functions. */
    wblprintf(LOG_INFO,"CORE","Registering functions from plugins (%d)...\n",num_cl_exts);
    for (i = 0; i < num_cl_exts; i++) {
        const char *str;
        char *cfg;
        function_t *cl;
        char *name;
        char *plugin;

        //Take the plugin identifier
        plugin=malloc(sizeof(char)*(strlen(cl_exts[i]->plugin->identifier)+1));
        strcpy(plugin,cl_exts[i]->plugin->identifier);

        // Get the function desc
        str = cp_lookup_cfg_value(
                cl_exts[i]->configuration, "@cfg"
        );

        if (str == NULL) {
            cfg = NULL;
        }
        else{
            cfg = malloc(sizeof(char)*strlen(str)+1);
            strcpy(cfg,str);
        }

        str = cp_lookup_cfg_value(
                cl_exts[i]->configuration, "@name"
        );

        if(str==NULL){
            cp_log(data->ctx, CP_LOG_ERROR,
                   "Ignoring function without name");
            name=NULL;
        }else{
            name = malloc((strlen(str)+1)*sizeof(char));
            strcpy(name,str);
        }

        // Resolve function data pointer
        str = cp_lookup_cfg_value(
        cl_exts[i]->configuration, "@function");
        if (str == NULL) {	
            // Classifier symbol name is missing
            cp_log(data->ctx, CP_LOG_ERROR,
                    "Ignoring function without symbol name.");
            continue;
        }

        cl = cp_resolve_symbol(
            data->ctx,
            cl_exts[i]->plugin->identifier,
            str,
            NULL
        );

        if (cl == NULL) {
            wblprintf(LOG_WARNING,"CORE","Could not resolve symbol: %s on %s\n",str,cl_exts[i]->plugin->identifier);
                // Could not resolve function symbol
                cp_log(data->ctx, CP_LOG_ERROR,
                        "Ignoring function which could not be resolved.");
                continue;
        }

        //Addd function name and function pointer to the function hashmap
        if(hashmap_put(functions_map,name,cl)!=MAP_OK){
            wblprintf(LOG_CRITICAL,"CORE","Not enough memory\n");
            exit(1);
        }
        if(hashmap_put(plugins_map,name,plugin)!=MAP_OK){
            wblprintf(LOG_CRITICAL,"CORE","Not enough memory\n");
            exit(1);
        }

        // Add function to the list of registered functions
        wblprintf(LOG_INFO,"CORE","Registering function %s (%s)...\n\t\t\t\t\tFunction name %s...\n",str,plugin,name);

        data->functions[data->num_functions].cfg = cfg;
        data->functions[data->num_functions].function = cl;
        data->functions[data->num_functions].name = name;
        data->functions[data->num_functions].plugin = plugin;
        //AKKKI
        (data->functions[data->num_functions].function->conf_function!=NULL)?(data->functions[data->num_functions].function->conf_function(cl->data,data->config_values)):(0);

        data->num_functions++;

    }

    // Release extension information
    cp_release_info(data->ctx, cl_exts);

///-----------------------------------------------------------------------

    //Get parser from es.uvigo.ei.core.parsers
    cl_exts = cp_get_extensions_info(
            data->ctx,
            "es.uvigo.ei.core.parsers",
            &status,
            &num_cl_exts
    );

    if (cl_exts == NULL) {
            wblprintf(LOG_CRITICAL,"CORE","Framework error on scanning parser extensions..\n");
            // An error occurred and framework logged it
            exit(-1);
            return status;
    }

    // Allocate memory for function information, if any
    //Reserva de memoria en la estructura data para el parser en caso de q se haya cogido alguno
    if (num_cl_exts > 0) {
            data->parsers = malloc(
                    num_cl_exts * sizeof(registered_parser_t)
            );
            if (data->parsers == NULL) {
                    // Memory allocation failed
                    cp_log(data->ctx, CP_LOG_ERROR,
                            "Insufficient memory for parser list.");
                    return CP_ERR_RESOURCE;
            }
            data->parsers_map=hashmap_new();
    }

    /* Resolve function parsers. This will implicitly start
     * plug-ins providing the parsers. */

    wblprintf(LOG_INFO,"CORE","Registering parsers from plugins (%d)...\n",num_cl_exts);
    for (i = 0; i < num_cl_exts; i++) {
      const char *str;
      char *cfg;
      parser_t *cl;
      char *name;

      // Get the parser desc
      str = cp_lookup_cfg_value(
               cl_exts[i]->configuration, "@cfg"
            );

      if(str == NULL){
         cfg=NULL;
      }
      else{
            cfg = malloc(sizeof(char)*strlen(str)+1);
            strcpy(cfg,str);
      }

      str = cp_lookup_cfg_value(
            cl_exts[i]->configuration, "@name"
      );

      if(str==NULL){
         cp_log(data->ctx, CP_LOG_ERROR,
                "Ignoring parser without name"
               );
         name=NULL;
      }
      else{
             name = malloc((strlen(str)+1)*sizeof(char));
             strcpy(name,str);
      }

      // Resolve function data pointer
      str = cp_lookup_cfg_value(
            cl_exts[i]->configuration, "@parser");
      if (str == NULL) {
          // Classifier symbol name is missing
          cp_log(data->ctx, CP_LOG_ERROR,
                 "Ignoring parser without symbol name."
                );
          continue;
      }
      cl = cp_resolve_symbol(data->ctx,
                             cl_exts[i]->plugin->identifier,
                             str,
                             NULL
                            );
      if (cl == NULL) {
          wblprintf(LOG_CRITICAL,"CORE","Could not resolve symbol: %s on %s\n",str,cl_exts[i]->plugin->identifier);
          //Could not resolve function symbol
          cp_log(data->ctx, CP_LOG_ERROR, "Ignoring parser which could not be resolved.");
          continue;
      }

      // Add parser to the list of registered parsers
      wblprintf(LOG_INFO,"CORE","Registering parser %s...\n",str);
      //printf("  Parser name %s...\n",name);

      if(hashmap_put(parsers_map,name,cl)!=MAP_OK){
        wblprintf(LOG_CRITICAL,"CORE","Not enough memory\n");
        exit(1);
      }


      data->parsers[data->num_parsers].cfg = cfg;
      data->parsers[data->num_parsers].parser = cl;
      data->parsers[data->num_parsers].name = name;

      hashmap_put(data->parsers_map,name,&data->parsers[data->num_parsers]);

      data->num_parsers++;

      //hashmap_iterate_keys(data->parsers_map,&it_data,NULL);
    }
    // Release extension information
    cp_release_info(data->ctx, cl_exts);


///-----------------------------------------------------------------------

    // Obtain list of registered eventhandlers
    cl_exts = cp_get_extensions_info(
            data->ctx,
            "es.uvigo.ei.core.eventhandlers",
            &status,
            &num_cl_exts
    );
    if (cl_exts == NULL) {
        wblprintf(LOG_CRITICAL,"CORE","Framework error on scanning eventhandler extensions..\n");
        // An error occurred and framework logged it
        exit(-1);
        return status;
    }

    // Allocate memory for function information, if any
    if (num_cl_exts > 0) {
        data->eventhandlers = malloc(
                num_cl_exts * sizeof(registered_eventhandler_t)
        );
        if (data->eventhandlers == NULL) {
                // Memory allocation failed
                cp_log(data->ctx, CP_LOG_ERROR,
                        "Insufficient memory for eventhandlers list.");
                return CP_ERR_RESOURCE;
        }
    } 

    /* Resolve function functions. This will implicitly start
     * plug-ins providing the functions. */
    wblprintf(LOG_INFO,"CORE","Registering eventhandlers from plugins (%d)...\n",num_cl_exts);
    for (i = 0; i < num_cl_exts; i++) {
        const char *str;
        char *cfg;
        eventhandler_t *cl;
        char *name;
        char *plugin;

        //Take the plugin identifier
        plugin=malloc(sizeof(char)*(strlen(cl_exts[i]->plugin->identifier)+1));
        strcpy(plugin,cl_exts[i]->plugin->identifier);

        // Get the function desc
        str = cp_lookup_cfg_value(
            cl_exts[i]->configuration, "@cfg"
        );

        if (str == NULL) {
            cfg = NULL;
        }
        else{
            cfg = malloc(sizeof(char)*strlen(str)+1);
            strcpy(cfg,str);
        }

        str = cp_lookup_cfg_value(
            cl_exts[i]->configuration, "@name"
        );

        if(str==NULL){
            cp_log(data->ctx, CP_LOG_ERROR,
            "Ignoring function without name");
            name=NULL;
        }else{
            name = malloc((strlen(str)+1)*sizeof(char));
            strcpy(name,str);
        }

        // Resolve function data pointer
        str = cp_lookup_cfg_value(
        cl_exts[i]->configuration, "@eventhandler");
        if (str == NULL) {
            // Classifier symbol name is missing
            cp_log(data->ctx, CP_LOG_ERROR,
                "Ignoring function without symbol name.");
            continue;
        }

        cl = cp_resolve_symbol(
            data->ctx,
            cl_exts[i]->plugin->identifier,
            str,
            NULL
        );

        if (cl == NULL) {
            wblprintf(LOG_WARNING,"CORE","Could not resolve symbol: %s on %s\n",str,cl_exts[i]->plugin->identifier);

            // Could not resolve function symbol
            cp_log(data->ctx, CP_LOG_ERROR,
                    "Ignoring function which could not be resolved.");
            continue;
        }
        // Add eventhandler to the list of registered functions
        wblprintf(LOG_INFO,"CORE","Registering event handler %s (%s)...\n",str,plugin);

        data->eventhandlers[data->num_eventhandlers].cfg = cfg;
        data->eventhandlers[data->num_eventhandlers].function = cl;
        data->eventhandlers[data->num_eventhandlers].name = name;
        data->eventhandlers[data->num_eventhandlers].plugin = plugin;
        data->num_eventhandlers++;
        
    }
        
        //--------------- PRESCHEDULERS
        
        // Obtain list of registered eventhandlers
        cl_exts = cp_get_extensions_info(
            data->ctx,
            "es.uvigo.ei.core.preschedulers",
            &status,
            &num_cl_exts
        );
        if (cl_exts == NULL) {
            wblprintf(LOG_CRITICAL,"CORE","Framework error on scanning prescheduler extensions..\n");
            // An error occurred and framework logged it
            exit(-1);
            return status;
        }

        // Allocate memory for function information, if any
        if (num_cl_exts > 0) {
            data->preschedulers = malloc(
                    num_cl_exts * sizeof(registered_prescheduler_t)
            );
            data->preschedulers_map=hashmap_new();
            
            if (data->preschedulers == NULL) {
                    // Memory allocation failed
                    cp_log(data->ctx, CP_LOG_ERROR,
                            "Insufficient memory for prescheduler list.");
                    return CP_ERR_RESOURCE;
            }
        } 

        /* Resolve function functions. This will implicitly start
         * plug-ins providing the functions. */
        wblprintf(LOG_INFO,"CORE","Registering preschedulers (%d)...\n",num_cl_exts);
        for (i = 0; i < num_cl_exts; i++) {
            const char *str;
            char *cfg;
            prescheduler_t *cl;
            char *name;

            // Get the function desc
            str = cp_lookup_cfg_value(
                cl_exts[i]->configuration, "@cfg"
            );

            if (str == NULL) {
                cfg = NULL;
            }
            else{
                cfg = malloc(sizeof(char)*strlen(str)+1);
                strcpy(cfg,str);
            }

            str = cp_lookup_cfg_value(
                cl_exts[i]->configuration, "@name"
            );

            if(str==NULL){
                cp_log(data->ctx, CP_LOG_ERROR,
                "Ignoring function without name");
                name=NULL;
            }else{
                name = malloc((strlen(str)+1)*sizeof(char));
                strcpy(name,str);
            }

            // Resolve function data pointer
            str = cp_lookup_cfg_value(
            cl_exts[i]->configuration, "@prescheduler");
            if (str == NULL) {
                // Classifier symbol name is missing
                cp_log(data->ctx, CP_LOG_ERROR,
                    "Ignoring function without symbol name.");
                continue;
            }

            cl = cp_resolve_symbol(
                data->ctx,
                cl_exts[i]->plugin->identifier,
                str,
                NULL
            );

            if (cl == NULL) {
                wblprintf(LOG_WARNING,"CORE","Could not resolve symbol: %s on %s\n",str,cl_exts[i]->plugin->identifier);

                // Could not resolve function symbol
                cp_log(data->ctx, CP_LOG_ERROR,
                        "Ignoring function which could not be resolved.");
                continue;
            }
            // Add eventhandler to the list of registered functions
            wblprintf(LOG_INFO,"CORE","Registering prescheduler %s...\n",str);

            data->preschedulers[data->num_preschedulers].cfg = cfg;
            data->preschedulers[data->num_preschedulers].function = cl;
            data->preschedulers[data->num_preschedulers].name = name;
            hashmap_put(data->preschedulers_map,name,&data->preschedulers[data->num_preschedulers]);
            data->num_preschedulers++;
    }

    // Release extension information
    cp_release_info(data->ctx, cl_exts);

///-----------------------------------------------------------------------	

    // Register run function to do the real work
    cp_run_function(data->ctx, run);

    if(get_attribute_values_ini(data->config_values,"CORE","lazy_evaluation", (void *)&value)>0){
        LAZY_EVALUATION = atof(value);
        //printf("LAZY VALUE: %2.2f\n",atof(value));
    }
    
    (LAZY_EVALUATION!=NONE)?
        (wblprintf(LOG_INFO,"CORE","LAZY EVALUATION ENABLED\n")):
        (wblprintf(LOG_INFO,"CORE","LAZY EVALUATION DISABLED\n"));
    
    //List all rule files.
    site_config_path_fl=list_files(rule_config_path,"cf");
    
    //Fill in the ruleset.
    data->rules=load_ruleset(site_config_path_fl,PROGRAM_MODE);
    
    if(data->rules==NULL){
        wblprintf(LOG_CRITICAL,"CORE","Uncharged rules. Exiting...\n");
        exit(EXIT_FAILURE);
    } else wblprintf(LOG_INFO,"CORE","All rules are loaded\n");

    //out_ruleset(data->rules);
    
    for(i=0;i<count_rules(data->rules);i++){
       void *parser_pointer;
       function_t *function_pointer;
       void *false_pointer=NULL;
       char *plugin_pointer=NULL;

       //printf("Getting parser name: %s\n",get_parser_name(data->rules,i));
       if(hashmap_get(parsers_map,get_parser_name(data->rules,i),(any_t *)&parser_pointer)==MAP_MISSING){// || parser_pointer==NULL){
           wblprintf(LOG_WARNING,"CORE","Parser '%s' does not exist... Ignoring rule\n",get_parser_name(data->rules,i));
       }else set_parser_pointer(data->rules,i,parser_pointer);           

       //printf("Getting definition name: %s\n",get_definition_name(data->rules,i));
       if(hashmap_get(functions_map,get_definition_name(data->rules,i),(any_t *)&function_pointer)==MAP_MISSING){// || function_pointer==NULL){
           wblprintf(LOG_WARNING,"CORE","Function '%s' does not exist... Ignoring rule\n",get_definition_name(data->rules,i));
           if(false_pointer==NULL) hashmap_get(functions_map,"false",(any_t *)&false_pointer);
           set_definition_pointer(data->rules,i,false_pointer);
           set_definition_plugin(data->rules,i,"es.uvigo.ei.false_plugin");
       }else{
           set_definition_pointer(data->rules,i,function_pointer);
           if (hashmap_get(plugins_map,get_definition_name(data->rules,i),(any_t *)&plugin_pointer)==MAP_MISSING){
              wblprintf(LOG_CRITICAL,"CORE","Unable to find plugin for function %s\n",get_definition_name(data->rules,i));
              exit(EXIT_FAILURE);
            }
           set_definition_plugin(data->rules,i,plugin_pointer);
       }
    }
    //--PRESCHEDULE
    char *plan_name;
    registered_prescheduler_t *registered_prescheduler;
    if(get_attribute_values_ini(data->config_values,"CORE","schedule",(void *)&plan_name)!=ELEMENT_MISSING){
        if(hashmap_get(data->preschedulers_map,plan_name,(any_t *)&registered_prescheduler)==MAP_MISSING){
            wblprintf(LOG_CRITICAL,"CORE","Schedule plan not found. Assuming default plan\n");
            hashmap_get(data->preschedulers_map,DEFAULT_SCHEDULE_PLAN,(any_t *)&registered_prescheduler);
            registered_prescheduler->function->function(registered_prescheduler->function->data,data->rules);                
        }
        else registered_prescheduler->function->function(registered_prescheduler->function->data,data->rules);
    }else{
        wblprintf(LOG_CRITICAL,"CORE","Schedule plan not assigned. Assuming default plan\n");
        hashmap_get(data->preschedulers_map,DEFAULT_SCHEDULE_PLAN,(any_t *)&registered_prescheduler);    
    }
 
    switch(LEARN_METHOD[0]){
        case 'n':
                 LEARN_TYPE=DISABLE_LEARN;
                 wblprintf(LOG_INFO,"CORE","Learning method disabled\n");
                 break;
        case 'h':
                 LEARN_TYPE=MANUAL_HAM;
                 wblprintf(LOG_INFO,"CORE","Manual learning method activated. Forced message learned as ham\n");
                 break;
        case 's':
                 LEARN_TYPE=MANUAL_SPAM;
                 wblprintf(LOG_INFO,"CORE","Manual learning method activated. Forced message learned as spam\n");                
                 break;
        default: 
                 LEARN_TYPE=AUTOLEARN;
                 wblprintf(LOG_INFO,"CORE","Auto learn method activated\n");                
                 break;
    }
    if(LEARN_METHOD!=NULL){
        free(LEARN_METHOD); 
        LEARN_METHOD=NULL;
    }
    // Successfully started
    hashmap_free(functions_map);
    hashmap_free(parsers_map);
    hashmap_free(plugins_map);

    free_filelist(site_config_path_fl);

    return CP_OK;
}

int free_executed_rules(any_t nullpointer, any_t data){
    free(data);
    return MAP_OK;
}

/**
 * Releases resources from other plug-ins.
 */
static void stop(void *d) {
     plugin_data_t *data = d;
     int i;
	// Release function data, if any

     
     if (data->functions != NULL) {
        // Release function pointers
        for (i=0; i < data->num_functions; i++) {

            cp_release_symbol(
                    data->ctx, data->functions[i].function
            );

            free(data->functions[i].plugin); //DAVID
            free(data->functions[i].cfg); //DAVID
            free(data->functions[i].name); //DAVID
        }

        // Free local data functions
        free(data->functions);
        data->functions = NULL;
        data->num_functions = 0;
     }
     
     if(data->executed_rules!=NULL){
        hashmap_iterate(data->executed_rules,&free_executed_rules,NULL);
        hashmap_free(data->executed_rules);
     }
         

     //ADDED David
     if(data->running_eventhandlers!=NULL){
         hashmap_iterate(data->running_eventhandlers,&pthread_join_data,NULL);
         hashmap_free(data->running_eventhandlers);
     }
     
     if(data->config_values!=NULL){
         free_ini(data->config_values);
        data->config_values=NULL;
     }
     
     if(data->parsedcontents!=NULL){
        hashmap_iterate_elements(data->parsedcontents,&free_parser_data,data->parsers_map);
        //Liberates de hashmap.
        hashmap_free(data->parsedcontents);
        data->parsedcontents=NULL;
     }
     
     //Release parses, if any
     if (data->parsers !=NULL){
        for(i=0;i<data->num_parsers;i++){
           cp_release_symbol(data->ctx, data->parsers[i].parser);
           //free(data->parsers[i].parser); //DAVID
           free(data->parsers[i].cfg); //DAVID
           free(data->parsers[i].name); //DAVID
        }
        //free local data parsers
        free(data->parsers);
        
        data->parsers = NULL;
        data->num_parsers = 0;
        
        hashmap_free(data->parsers_map); //DAVID

     }
     
     //ADDED: DAVID.
     if (data->eventhandlers !=NULL){
        for(i=0;i<data->num_eventhandlers;i++){
           cp_release_symbol(data->ctx, data->eventhandlers[i].function);
           free(data->eventhandlers[i].cfg);
           free(data->eventhandlers[i].name);
           free(data->eventhandlers[i].plugin);
        }
        //free local data parsers
        free(data->eventhandlers);
        data->eventhandlers = NULL;
        data->num_eventhandlers = 0;
     }
     //ADDED: DAVID.

     if(data->preschedulers!=NULL){
         for(i=0;i<data->num_preschedulers;i++){
             cp_release_symbol(data->ctx,data->preschedulers[i].function);
             free(data->preschedulers[i].cfg);
             free(data->preschedulers[i].name);
         }
         free(data->preschedulers);
         data->preschedulers = NULL;
         data->num_preschedulers = 0;
         
         hashmap_free(data->preschedulers_map);
     }
     //cp_destroy_context(data->ctx);
     if(data->stored_rules!=NULL) free(data->stored_rules);
     if(data->to_domain!=NULL) free_eml_domains(data->to_domain);
     //printf("ACORDARSE LIBERAR TO_DOMAIN\n");
     //if(data->positive_score!=NULL) free(data->positive_score);
     
     //if(data->negative_score!=NULL) free(data->negative_score);

     data->stored_rules=NULL;
     data->score=0;

     ruleset_free(data->rules);

}

/**
 * Destroys a plug-in instance.
 */
static void destroy(void *d) {
   free(d);
}


int free_parser_data(any_t parsers_map, any_t data, any_t key){
    
    free_func_t function;
    registered_parser_t *parser;

    //printf("KEY PARSED_CONTENTS: %s\n",(char *)key);

    hashmap_get((map_t)parsers_map,(char *)key, (any_t *)&parser);

    function=parser->parser->free_parser_data;
    
    function(data);
    
    return MAP_OK;
}


int pthread_join_data(any_t nullpointer, any_t data){
   pthread_t *hilo=(pthread_t *)data;
   pthread_join(*hilo,NULL);
   pthread_detach(*hilo);
   free(hilo);
   return MAP_OK;
}



/* ------------------------------------------------------------------------
 * Exported runtime information
 * ----------------------------------------------------------------------*/

/**
 * Plug-in runtime information for the framework. The name of this symbol
 * is stored in the plug-in descriptor.
 */
CP_EXPORT cp_plugin_runtime_t core_runtime_functions = {
    create,
    start,
    stop,
    destroy
};
