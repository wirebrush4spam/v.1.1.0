/***************************************************************************                        
*
*   File    : ruleset.c
*   Purpose : Implements a ruleset (set of rules) for filtering
*            
*   Original Author: Ivan Paz, Jose Ramon Mendez (from Grindstone project)
*   Has been widelly modifyed since them
* 
*   Memory improvements, modifications, inclusion of new fields
*       and functions: David Ruano, Noemi Perez, Jose Ramon Mendez
* 
*   Date    : October  14, 2010
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ruleset.h"
#include "string_util.h"
#include "list_files.h"
#include "logger.h"
#include "hashmap.h"
#include "parse_func_args.h"
#include "meta.h"

/***********************************************************************
 Datatypes
 **********************************************************************/

int debug_mode=0;

//struct stop_score {
//    float upper;
//    float lower;
//};

//typedef struct stop_score stop_score;



/***********************************************************************
 Private Function List
***********************************************************************/

/**
 * Indicates if line is commented
 */
int is_commented(char *line);

/**
 * Update a ruleset map when created from another ruleset
 * (Combine and Mutate operations)
 */
void update_map(ruleset *r);

/**
 * Comparator for sorting rules by plugin
 */
int compare_rules_by_plugin(const void *_a, const void *_b);

int fprint_domains(any_t file,any_t key);

int print_domains(any_t nullpointer,any_t key);

int free_meta_pos(any_t item, any_t data);

int free_vector_element(element data);

/***********************************************************************
 Private and Public functions Implementation
***********************************************************************/ 

/* count the rules on a ruleset */
int count_rules(const ruleset *rules){
   return rules->size;
}

int count_definitive_rules(const ruleset *rules){
   return rules->def_size;
}

/* count the number of meta rules on a ruleset */
int count_meta_rules(const ruleset *rules){
   return rules->meta_size;
}
/* load a ruleset */
ruleset *load_ruleset(const filelist *files,int program_mode){
   int rulenum=0, meta_num=0;
   char ch;
   char *line;
   FILE *fp;
   ruleset *r=NULL;
   char *tmpstr,*tmptoken;
   int i; int *rulepos;
   int initparameters,endparameters;
   char *tmpDefName, *tmpDefParams;   
   
   if (count_files_filelist(files)==0) return r;

   //Initialize ruleset 
   if((r=(ruleset *)malloc(sizeof(ruleset)))==NULL){
       wblprintf(LOG_CRITICAL,"RULESET","Insuficient Memory\n");
       exit(1);
   }   

   r->map= hashmap_new();
   r->meta_map= hashmap_new();
   r->dependant_map = hashmap_new();
   //r->def_map= hashmap_new();
   
   r->required=0;
      
   //Initialize
   if((line=(char *)malloc(sizeof(char)))==NULL){
      wblprintf(LOG_CRITICAL,"RULESET","Insuficient Memory\n");
      exit(1);
   }
   strcpy(line,"");
   //Compute the number of rules in order to detect the size of the
   //ruleset
   for(i=0;i<count_files_filelist(files);i++){
         //printf("lista[%d]= %s\n",i,get_file_at(files,i));
         //printf("Opening %s.\n", get_file_at(files,i));
         fp=fopen(get_file_at(files,i),"r");
         if (fp==NULL) {
             wblprintf(LOG_CRITICAL,"RULESET","Unable to open file %s. Exiting...\n",get_file_at(files,i));
             exit(-1);
         }
         do {
            ch=fgetc(fp);
            if(ch=='\n' || ch==EOF) {
                if (strcmp(trim(line),"")==0) continue; //Discard line if void
                //printf("end line: %s\n",line);
                if (!is_commented(line)){
                     tmpstr=strtok(line," \t");
                     if (strcmp(tmpstr,"body")==0 || strcmp(tmpstr,"header")==0 ||
                         strcmp(tmpstr,"url")==0 || strcmp(tmpstr,"full")==0 ){
                         
                         if(program_mode==CMS_FILTER){ 
                             wblprintf(LOG_CRITICAL,"RULESET","Cannot compute header/body/url rules in CMS platform. Ignoring rule...\n");
                         }
                         else{
                             tmpstr=strtok(NULL," \t"); //Take rulename in tmpstr
                             tmptoken=malloc(sizeof(char)*(1+strlen(tmpstr)));
                             strcpy(tmptoken,tmpstr);
                             if (get_rule_index(r,tmptoken)==-1){ //If rule is not included
                                 rulepos=malloc(sizeof(int));
                                 *rulepos=rulenum;
                                 hashmap_put(r->map,tmptoken,rulepos);
                                 rulenum++;
                                 wblprintf(LOG_INFO,"RULESET","Inserting rulename '%s'\n",tmptoken);

                             }else wblprintf(LOG_WARNING,"RULESET","WARNING: rule definition repeated %s at position %i\n",tmpstr,get_rule_index(r,tmpstr));     
                         }
                     }else{
                         if(strcmp(tmpstr,"txt")==0){
                             if(program_mode==SPAM_FILTER){ 
                                 wblprintf(LOG_CRITICAL,"RULESET","Cannot compute txt rules in SPAM platform. Ignoring rule...\n");
                             }
                             else{
                                 tmpstr=strtok(NULL," \t"); //Take rulename in tmpstr
                                 tmptoken=malloc(sizeof(char)*(1+strlen(tmpstr)));
                                 strcpy(tmptoken,tmpstr);
                                 if (get_rule_index(r,tmptoken)==-1){ //If rule is not included
                                     rulepos=malloc(sizeof(int));
                                     *rulepos=rulenum;
                                     hashmap_put(r->map,tmptoken,rulepos);
                                     rulenum++;
                                     wblprintf(LOG_INFO,"RULESET","Inserting rulename '%s'\n",tmptoken);

                                 }else wblprintf(LOG_WARNING,"RULESET","WARNING: rule definition repeated %s at position %i\n",tmpstr,get_rule_index(r,tmpstr));     
                             }                               
                         }
                         else{
                             if(strcmp(tmpstr,"meta")==0){
                                 tmpstr=strtok(NULL," \t"); //Take rulename in tmpstr
                                 tmptoken=malloc(sizeof(char)*(strlen(tmpstr)+1));
                                 strcpy(tmptoken,tmpstr);
                                 if(get_meta_index(r,tmptoken)==-1){
                                     rulepos=malloc(sizeof(int));
                                     *rulepos=rulenum;
                                     hashmap_put(r->meta_map,tmptoken,rulepos);
                                     rulenum++;
                                     meta_num++;
                                     wblprintf(LOG_INFO,"RULESET","Inserting meta rulename '%s' \n",tmptoken);
                                 }else printf("WARNING: rule definition repeated %s at position %i\n",tmpstr,get_rule_index(r,tmpstr));
                             }
                         }
                     }
                     tmptoken=NULL;
                }
                free(line);
                line=(char *)malloc(sizeof(char));
                strcpy(line,"");
            }else{
                line=append(line,ch);
            }
         } while (ch!=EOF);
         fclose(fp);
   }
   if(rulenum==0){ 
       wblprintf(LOG_CRITICAL,"RULESET","Filter is empty, no rules are defined. Aborting...\n");
       exit(EXIT_FAILURE);
   }
   
   r->rules=(rule *)malloc(sizeof(rule)*(rulenum));
   r->size=rulenum;
   
   for (i=0;i<r->size;i++){
        r->rules[i].def=NULL;
        r->rules[i].par=NULL;
        r->rules[i].rulename=NULL;
        r->rules[i].score=NULL;
        r->rules[i].description=NULL;
        r->rules[i].target_domain=NULL;
        r->rules[i].characteristic=0;
   }
   
   r->sdata_t=malloc(sizeof(schedule_data_t));
   r->sdata_t->end_definitive=0;
   r->sdata_t->end_meta=0;
   r->sdata_t->begin_normal=0;
   
   r->def_size=0;
   r->meta_size=meta_num;
   
   //r->stop=(stop_score *)malloc(sizeof(stop_score));
   r->intervals=(score_intervals *)malloc(sizeof(score_intervals));
   r->intervals->positive=0;
   r->intervals->negative=0;
   
   //Load rules from file
   rulenum=0;
   free(line);
   line=(char *)malloc(sizeof(char));
   //strcpy(line,"");
   line[0]='\0';
   char *linecopy=NULL;

   for(i=0;i<count_files_filelist(files);i++){
       fp=fopen(get_file_at(files,i),"r");
       do {
         ch=fgetc(fp);
         if(ch=='\n' || ch == EOF) {
           if (strcmp(trim(line),"")==0) continue; //Descartar la linea si está vacia
           //printf("Working with line: %s\n",line);
           if (!is_commented(line)){
              //printf("Working with line: %s\n",line);
              //PARSER NAME
              int tam=strlen(line);

              linecopy=malloc(sizeof(char)*(tam+1));
              strcpy(linecopy,line);

              tmpstr=strtok(line," \t");

              if (strcmp(tmpstr,"body")==0 || strcmp(tmpstr,"header")==0 ||
                  strcmp(tmpstr,"url")==0 || strcmp(tmpstr,"full")==0){

                  tmpstr=strtok(NULL," \t");
                  //SI LA REGLA NO ESTA EN EL HASHMAP
                  
                  if(program_mode==SPAM_FILTER){
                  
                      if((rulenum=get_rule_index(r,tmpstr))!=-1){
                        set_parser(r,rulenum,line);//,EML_PARSER);
                        r->rules[rulenum].characteristic = NORMAL_RULE;
                        r->rules[rulenum].rulename=hashmap_getkeypointer(r->map,tmpstr); 

                        //Next token: parser definition
                        tmpstr=strtok(NULL," \t");

                        //Separar el nombre de la funcion de los parametros.

                        tmptoken=strtok(tmpstr,"(");

                        tmpDefName= (char *)malloc((strlen(tmptoken)+1)*sizeof(char));
                        strcpy(tmpDefName,tmptoken);

                        for(initparameters=0;linecopy[initparameters]!='(' && linecopy[initparameters]!='\0';initparameters++);

                        initparameters++;

                        for(endparameters=(strlen(linecopy)-1);linecopy[endparameters]!=')' && endparameters>=0;endparameters--);
                        if(endparameters-initparameters!=0){
                            tmpDefParams=malloc(sizeof(char)*(endparameters-initparameters+1));
                            memcpy(tmpDefParams, &(linecopy[initparameters]), endparameters-initparameters);
                            tmpDefParams[endparameters-initparameters]='\0';
                        }else tmpDefParams=NULL;

                        set_definition(r,rulenum,tmpDefName,tmpDefParams);
                        free(tmpDefParams);
                        free(tmpDefName);

                      }
                      else{
                        wblprintf(LOG_CRITICAL,"RULESET","Rules changed during starting process... exiting\n");
                        exit(EXIT_FAILURE);
                      }
                  }
              }
              else
                  if(strcmp(tmpstr,"txt")==0){
                      
                      if(program_mode==CMS_FILTER){
                      
                          tmpstr=strtok(NULL," \t");
                          if((rulenum=get_rule_index(r,tmpstr))!=-1){
                            set_parser(r,rulenum,line);//,TXT_PARSER);

                            //set_rulename(r,rulenum,tmpstr); //Moncho:: Optimize memory and use the hashmap key
                            r->rules[rulenum].rulename=hashmap_getkeypointer(r->map,tmpstr); 
                            r->rules[rulenum].characteristic = NORMAL_RULE;
                            //Next token: parser definition
                            tmpstr=strtok(NULL," \t");

                            //Separar el nombre de la funcion de los parametros.

                            tmptoken=strtok(tmpstr,"(");

                            tmpDefName= (char *)malloc((strlen(tmptoken)+1)*sizeof(char));
                            strcpy(tmpDefName,tmptoken);

                            for(initparameters=0;linecopy[initparameters]!='(' && linecopy[initparameters]!='\0';initparameters++);

                            initparameters++;

                            for(endparameters=(strlen(linecopy)-1);linecopy[endparameters]!=')' && endparameters>=0;endparameters--);
                            if(endparameters-initparameters!=0){
                                tmpDefParams=malloc(sizeof(char)*(endparameters-initparameters+1));
                                memcpy(tmpDefParams, &(linecopy[initparameters]), endparameters-initparameters);
                                tmpDefParams[endparameters-initparameters]='\0';
                            }else tmpDefParams=NULL;

                            set_definition(r,rulenum,tmpDefName,tmpDefParams);
                            free(tmpDefParams);
                            free(tmpDefName);
                          }
                          else{
                            wblprintf(LOG_CRITICAL,"RULESET","Rules changed during starting process... exiting\n");
                            exit(EXIT_FAILURE);
                          } 
                      }
                  }
                  else if(strcmp(tmpstr,"meta")==0){
                          tmpstr=strtok(NULL," \t");

                          if((rulenum=get_meta_index(r,tmpstr))==-1){
                            wblprintf(LOG_CRITICAL,"RULESET","Meta rules changed during starting process... exiting\n");
                            exit(EXIT_FAILURE);
                          }                  
                          //r->meta_rules[rulenum].rulename=hashmap_getkeypointer(r->meta_map,tmpstr);
                          r->rules[rulenum].rulename=hashmap_getkeypointer(r->meta_map,tmpstr);
                          r->rules[rulenum].characteristic = META_RULE;
                          
                          char *aux=NULL;
                          while((tmpstr=strtok(NULL, "\t"))!=NULL) aux=tmpstr;                  

                          //set_meta_expression(r,rulenum,aux);
                          //char *expression=malloc(sizeof(char)*(strlen(aux)+1));
                          //strcpy(expression,aux);
                          set_meta_definition(r,rulenum,aux);
                          //r->rules[rulenum].def=expression;

                      }else 
                          if (strcmp(tmpstr,"score")==0) {
                            tmpstr=strtok(NULL," \t"); //Take the rulename
                            if (tmpstr!=NULL) {
                                if((rulenum=get_rule_index(r,tmpstr))!=-1 ||
                                   (rulenum=get_meta_index(r,tmpstr))!=-1 ) {
                                  tmpstr=strtok(NULL," \t"); //Take the rulescore
                                  if(strcmp(tmpstr,"+")==0 || strcmp(tmpstr,"-")==0){
                                      //r->rules[rulenum].definitive_value=malloc(sizeof(char)*(strlen(tmpstr)+1));
                                      r->rules[rulenum].characteristic |= DEFINITIVE_SCORE;
                                      char *score=malloc(sizeof(char)*(strlen(tmpstr)+1));
                                      strcpy(score,tmpstr);
                                      r->rules[rulenum].score=score;
                                      //strcpy(r->rules[rulenum].definitive_value,tmpstr);
                                      //r->rules[rulenum].score=malloc(sizeof(float));
                                      //*r->rules[rulenum].score=0;
                                      r->def_size++;
                                  }
                                  else{
                                      //r->rules[rulenum].score=malloc(sizeof(float)); //Take space for a float
                                      //*(r->rules[rulenum].score)=atof(tmpstr); //Assign the rulescore
                                      float *score = malloc(sizeof(float));
                                      *score=atof(tmpstr);
                                      r->rules[rulenum].score=(void *)score;
                                      r->rules[rulenum].characteristic |= NORMAL_SCORE;
                                      //(*(r->rules[rulenum].score)>0)?
                                      (*(score)>0)?
                                        (r->intervals->positive+=*(score)):
                                        (r->intervals->negative+=*(score));
                                  }

                                }

                            } else wblprintf(LOG_WARNING,"RULESET","Unable to parse score: %s\n",line);

                          }else{ 
                              if (strcmp(tmpstr,"describe")==0) { //Revisar
                                tmpstr=strtok(NULL," \t");
                                if(tmpstr!=NULL){                         
                                    if ((rulenum=get_rule_index(r,tmpstr))!=-1 || 
                                        (rulenum=get_meta_index(r,tmpstr))!=-1 ){
                                      for( tmpstr=linecopy; (tmpstr[0] != ' ') && (tmpstr[0] != '\t');tmpstr=&tmpstr[1]);
                                          tmpstr=trim(tmpstr);                          
                                          r->rules[rulenum].description=malloc(sizeof(char)*(1+strlen(tmpstr))); //Take space for the string
                                          strcpy(r->rules[rulenum].description,tmpstr);
                                    } 
                                }else wblprintf(LOG_WARNING,"RULESET","Unable to parse description: %s\n",line);

                                 //printf("tmpstr: %s, line: %s\n",tmpstr,line);
                              }else{ 
                                  if (strcmp(tmpstr,"required_score")==0) {
                                      tmpstr=strtok(NULL," \t"); //Take the score
                                      r->required=atof(tmpstr); //Assign the score
                                  }
                                  else{ 
                                      if(strcmp(tmpstr,"domain")==0 && program_mode!=CMS_FILTER){
                                          tmpstr=strtok(NULL," \t");
                                          if(tmpstr!=NULL){
                                              if((rulenum=get_rule_index(r,tmpstr)!=-1) || 
                                                 (rulenum=get_meta_index(r,tmpstr)!=-1) ){
                                                   (r->rules[rulenum].target_domain==NULL)?
                                                        (r->rules[rulenum].target_domain=hashmap_new()):
                                                        (0);
                                                    while((tmpstr=strtok(NULL,"\n\t "))!=NULL){
                                                        any_t result;
                                                        if(hashmap_get(r->rules[rulenum].target_domain,tmpstr,&result)==MAP_MISSING){
                                                            char *domain=malloc(sizeof(char)*(strlen(tmpstr)+1));
                                                            strcpy(domain,tmpstr);
                                                            hashmap_put(r->rules[rulenum].target_domain,domain,NULL);
                                                        } else wblprintf(LOG_WARNING,"RULESET","Domain %s already inserted: %s\n",tmpstr);
                                                    }
                                              } 
                                          } 
                                          else wblprintf(LOG_WARNING,"RULESET","Unable to parse domain section: %s\n",line);
                                      }
                                  }
                              }
                          }
                free(linecopy);
           }
           free(line);
           line=(char *)malloc(sizeof(char));
           line[0]='\0';
           
         }else line=append(line,ch);
         
       }while (ch!=EOF);
       
       fclose(fp);
   }

   free(line);
   
   for(i=0;i<r->size;i++){
      if(r->rules[i].score==NULL){
         wblprintf(LOG_WARNING,"RULESET","Rule %s without score\n\t\t\t\t\t      Assigning default score\n",r->rules[i].rulename);
         float *score=malloc(sizeof(float));
         *score=0;
         (r->rules[i].score)=(void *)score;
      }
   }

   return r;
}

/* Mutate operation for genetic algorithms */
ruleset *mutate(const ruleset *rulesin){
   int i,j;
   int veces;
   ruleset *r;
   double aleat;

   //Generar aleatorio
   //rand()
   //Generar aleatorio entre 0 y n-1 que corresponde a generar un aleatorio
   //para mutar la regla i en un conjunto de reglas de tamaño n (primera regla
   //la regla 0, última regla n-1
   //rand() % n

   //Initialize and copy ruleset 
   r=(ruleset *)malloc(sizeof(ruleset));
   r->required=rulesin->required;
   r->rules=(rule *)malloc(sizeof(rule)*rulesin->size);
   r->size=rulesin->size;
   //TODO: Copy the rest of fields
   for (i=0;i<r->size;i++){
      if(r->rules[i].characteristic & NORMAL_RULE & NORMAL_SCORE){
          *((float *)r->rules[i].score)=*((float *)rulesin->rules[i].score);
          r->rules[i].rulename=(char *)malloc(sizeof(char)*(strlen(rulesin->rules[i].rulename)+1));
          strcpy(r->rules[i].rulename,rulesin->rules[i].rulename);
      }
   }
 
   veces=rand() % (r->size -1)+1;
   for (j=0;j<veces;j++){
       if(r->rules[i].characteristic & NORMAL_RULE & NORMAL_SCORE){
           i=rand() % r->size;
           aleat=0.5*((rand()%10) + 1);
           //printf("Número generado %f\n",aleat);
           (rand() % 2)?
               (*((float *)r->rules[i].score)+=aleat):
               (*((float *)r->rules[i].score)-=aleat);
       }
   }

   //update the map in order to find rules by name
   update_map(r);
   return r; 
}

/* Combine operation for genetic algorithms */
ruleset *combine(const ruleset *rules1,const ruleset *rules2){
   int i;
   ruleset *r;

   //Generar aleatorio
   //rand()
   //Generar aleatorio entre 0 y n-1 que corresponde a generar un aleatorio
   //para mutar la regla i en un conjunto de reglas de tamaño n (primera regla
   //la regla 0, última regla n-1
   //rand() % n

   //Initialize and copy ruleset 
   r=(ruleset *)malloc(sizeof(ruleset));
   r->required=rules1->required;
   r->rules=(rule *)malloc(sizeof(rule)*rules1->size);
   r->size=rules1->size;
   //TODO: Copy the rest of fields

   for (i=0;i<r->size;i++){
       
      if(r->rules[i].characteristic & NORMAL_RULE & NORMAL_SCORE){ 
          r->rules[i].rulename=(char *)malloc(sizeof(char)*(strlen(rules1->rules[i].rulename)+1));
          strcpy(r->rules[i].rulename,rules1->rules[i].rulename);

          switch(rand()%10){
             case 0: *((float *)r->rules[i].score)=*((float *)rules1->rules[i].score);
                break;
             case 1: *((float *)r->rules[i].score)=*((float *)rules2->rules[i].score);
                break;
             case 2: *((float *)r->rules[i].score)=(*((float *)rules1->rules[i].score) + *((float *)rules2->rules[i].score)) / (float)2;
               break;
             case 3:
             case 7: *((float *)r->rules[i].score)=(*((float *)rules1->rules[i].score) > *((float *)rules2->rules[i].score))?*((float *)rules1->rules[i].score):*((float *)rules2->rules[i].score);
               break;
             case 4:*((float *)r->rules[i].score)=(*((float *)rules1->rules[i].score) < *((float *)rules2->rules[i].score))?*((float *)rules1->rules[i].score):*((float *)rules2->rules[i].score);
               break;
             case 5:
             case 8:*((float *)r->rules[i].score)=(rand()%2)?(*((float *)rules1->rules[i].score) - (0.1*((rand()%10) + 1))):(*((float *)rules1->rules[i].score) + (0.1*((rand()%10) + 1)));
               break;
             case 6:
             case 9:*((float *)r->rules[i].score)=(rand()%2)?(*((float *)rules2->rules[i].score) - (0.1*((rand()%10) + 1))):(*((float *)rules2->rules[i].score) + (0.1*((rand()%10) + 1)));
               break;
          }
      }
   }

   update_map(r);
   return r;
}

/* Write a ruleset in a file */
void write_ruleset(const char* filename, const ruleset *rules){
    FILE *file=fopen(filename,"w");
    int i;
    map_t rule_domain;
    fprintf(file,"#Ruleset output (size=%d)\n",rules->size);
    for (i=0;i<rules->size;i++){
        fprintf(file,"#Rule %d\n",i);
        if(rules->rules[i].characteristic & NORMAL_RULE){
            if(get_definition_param(rules,i)!=NULL)
               fprintf(file,"%s %s %s(%s)\n",get_parser_name(rules,i),get_rulename(rules,i),
                      get_definition_name(rules,i), get_definition_param(rules,i));
            else
               fprintf(file,"%s %s %s()\n",get_parser_name(rules,i),get_rulename(rules,i),
                      get_definition_name(rules,i));


            (get_tflags(rules,i)!=NULL)?(fprintf(file,"tflags %s\n",get_tflags(rules,i))):(0);
        }else fprintf(file,"meta %s %s\n",get_rulename(rules,i),get_meta_definition(rules,i));

        (rules->rules[i].characteristic & NORMAL_SCORE)?
            (fprintf(file,"score %s %.2lf\n",get_rulename(rules,i),*((float *)get_rule_score(rules,i)))):
            (fprintf(file,"score %s %s\n",get_rulename(rules,i),(char *)get_rule_score(rules,i)));

        if (get_description(rules,i)!=NULL)
            fprintf(file,"describe %s %s\n\n",get_rulename(rules,i),get_description(rules,i));

        if((rule_domain=get_rule_domain(rules,i))!=NULL){
           fprintf(file,"domain");
           hashmap_iterate_keys(rules->map, &print_domains, NULL);
           fprintf(file,"\n");
        }
    }
    
    fclose(file);
}

/* Write the ruleset in standard output */
void out_ruleset(const ruleset *rules){
    int i;
    map_t rule_domain;
    printf("#Ruleset output (size=%d)\n",rules->size);
    for (i=0;i<rules->size;i++){
        printf("#Rule %d\n",i);
        if(rules->rules[i].characteristic & NORMAL_RULE){
            if(get_definition_param(rules,i)!=NULL)
               printf("%s %s %s(%s)\n",get_parser_name(rules,i),get_rulename(rules,i),
                      get_definition_name(rules,i), get_definition_param(rules,i));
            else
               printf("%s %s %s()\n",get_parser_name(rules,i),get_rulename(rules,i),
                      get_definition_name(rules,i));


            (get_tflags(rules,i)!=NULL)?(printf("tflags %s\n",get_tflags(rules,i))):(0);
        }else printf("meta %s %s\n",get_rulename(rules,i),get_meta_definition(rules,i));

        (rules->rules[i].characteristic & NORMAL_SCORE)?
            (printf("score %s %.2lf\n",get_rulename(rules,i),*((float *)get_rule_score(rules,i)))):
            (printf("score %s %s\n",get_rulename(rules,i),(char *)get_rule_score(rules,i)));

        if (get_description(rules,i)!=NULL)
            printf("describe %s %s\n\n",get_rulename(rules,i),get_description(rules,i));

        if((rule_domain=get_rule_domain(rules,i))!=NULL){
           printf("domain");
           hashmap_iterate_keys(rules->map, &print_domains, NULL);
           printf("\n");
        }
    }
}

/*Get required score.*/
float get_required_score(const ruleset *rules){
   return rules->required;
}

int print_domains(any_t nullpointer,any_t key){
    printf(" %s",(char *)key);
    return MAP_OK;
}

int fprint_domains(any_t file,any_t key){
    fprintf(file," %s",(char *)key);
    return MAP_OK;
}

/*Get the name of a certain rule*/
char * get_rulename(const ruleset *rules,const int ruleno){
    return rules->rules[ruleno].rulename;
}

/*Get the rule definition struct.*/
definition *get_definition(const ruleset *rules,const int ruleno){
    return rules->rules[ruleno].def;
}

char *get_meta_definition(const ruleset *rules,const int ruleno){
    return ((meta_definition *)rules->rules[ruleno].def)->expresion;
}

/*Get the rule definition name.*/
char *get_definition_name(const ruleset *rules,const int ruleno){
    return ((definition *)rules->rules[ruleno].def)->name;
}

/*Get the rule definition param.*/
char *get_definition_param(const ruleset *rules,const int ruleno){
    return ((definition *)(rules->rules[ruleno].def))->param;
}

/*Get the rule definition pointer.*/
function_t *get_definition_pointer(const ruleset *rules,const int ruleno){
    return ((definition *)(rules->rules[ruleno].def))->pointer;
}

/*Get the rule tFlags*/
char *get_tflags(const ruleset *rules,const int ruleno){
    return ((definition *)(rules->rules[ruleno].def))->tflags;
}

/*Get the rule parser.*/
parser *get_parser(const ruleset *rules,const int ruleno){
    return rules->rules[ruleno].par;
}

/*Get the rule parser name*/
char *get_parser_name(const ruleset *rules, const int ruleno){
    return rules->rules[ruleno].par->parserName;
}

/*Get the rule parser name*/
//int get_parser_type(const ruleset *rules, const int ruleno){
//    return rules->rules[ruleno].par->parser_type;
//}

/*Get the rule parser name*/
map_t get_rule_domain(const ruleset *rules, const int ruleno){
    return rules->rules[ruleno].target_domain;
}

/*Get the rule parser name*/
//map_t get_meta_domain(const ruleset *rules, const int ruleno){
//    return rules->meta_rules[ruleno].target_domain;
//}

/*Get the rule parser pointer*/
parser_t *get_parser_pointer(const ruleset *rules,const int ruleno){
    return rules->rules[ruleno].par->parserPointer;
}

//char *get_definitive_score(const ruleset *rules, int ruleno){
//    return (rules->rules[ruleno].definitive_value);
//}

/*Return the score for a certain rule*/
void *get_rule_score(const ruleset *rules, int ruleno){
   return rules->rules[ruleno].score;
}

//char *get_definitive_score(const ruleset *rules, int ruleno){
//   return rules->rules[ruleno].score;
//}

//float get_meta_score(const ruleset *rules, int metano){
//    return *(rules->meta_rules[metano].score);
//}

float get_positive_score(const ruleset *rules){
    return rules->intervals->positive;
}

short get_rule_characteristic(const ruleset *rules, const int ruleno){
    return rules->rules[ruleno].characteristic;
}

float get_negative_score(const ruleset *rules){
    return rules->intervals->negative;
}

int free_domains(any_t nullpointer,any_t key){
    free(key);
    return MAP_OK;
}

/*Free the ruleset struct from memory.*/
void ruleset_free(ruleset *r){
    int i;
    int *pi;
    for (i=0;i<r->size;i++){
	   //printf("liberating %s\n",r->rules[i].rulename);
       hashmap_get(r->map,r->rules[i].rulename, (any_t *)&pi);
       hashmap_remove(r->map, r->rules[i].rulename);
       free(pi);
       (r->rules[i].rulename!=NULL)?(free(r->rules[i].rulename)):(0);
       (r->rules[i].score!=NULL)?(free(r->rules[i].score)):(0);
       (r->rules[i].description!=NULL)?(free(r->rules[i].description)):(0);
       //(r->rules[i].definitive_value!=NULL)?(free(r->rules[i].definitive_value)):(0);
       //if (r->rules[i].target_domain!=NULL) free(r->rules[i].target_domain);
       //printf(".\n");
       if (r->rules[i].def!=NULL){
           if(r->rules[i].characteristic & NORMAL_RULE){
             (((definition *)(r->rules[i].def))->name!=NULL)?
                 (free((((definition *)(r->rules[i].def))->name))):
                 (0);
             //printf("+\n");
             (((definition *)(r->rules[i].def))->param!=NULL)?
                 (free((((definition *)(r->rules[i].def))->param))):(0);
             //printf("+\n");
             (((definition *)(r->rules[i].def))->plugin!=NULL)?
                 (free((((definition *)(r->rules[i].def))->plugin))):
                 (0);
             //printf("+\n");
             (((definition *)(r->rules[i].def))->tflags!=NULL)?
                 (free((((definition *)(r->rules[i].def))->tflags))):
                 (0);
             //printf("+\n");
           }else{
               ( ((meta_definition*)(r->rules[i].def))->expresion!=NULL )?
                   ( free( ((meta_definition *)(r->rules[i].def))->expresion) ):
                   (0);
               if(((meta_definition*)(r->rules[i].def))->dependant_rules!=NULL ){
                   vector *aux=((meta_definition*)(r->rules[i].def))->dependant_rules;
                   free_meta_dependant_rules(aux);
               }
           }
           free(r->rules[i].def);
       }

       if(r->rules[i].target_domain!=NULL){
           hashmap_iterate_keys(r->rules[i].target_domain,&free_domains,NULL);
           hashmap_free(r->rules[i].target_domain);
       }
       if (r->rules[i].par!=NULL){
           (r->rules[i].par->parserName!=NULL)?(free(r->rules[i].par->parserName)):(0);
           free(r->rules[i].par);
       }
    }

    if(r->sdata_t!=NULL) free(r->sdata_t);
    
    hashmap_free(r->dependant_map);
    
    if(r->intervals!=NULL){
        free(r->intervals);
        r->intervals=NULL;
    }
    
    hashmap_free(r->map);
    hashmap_iterate(r->meta_map,&free_meta_pos,NULL);
    hashmap_free(r->meta_map);
    free(r->rules);

    free(r);
}

int free_meta_pos(any_t item, any_t data){
    free(data);
    return MAP_OK;
}

/*Evaluate if a line is commented*/
int is_commented(char *line){
   char *copia;
   int ret;
   char *firstTok;

   copia=(char *)malloc((strlen(line)+1)*sizeof(char));   
   strcpy(copia,line);
   firstTok=strtok(copia," \t");
   ret=((strcmp("//",firstTok)==0) || strchr(copia,'#')!=NULL);
   free(copia);

   return ret;
}

/*Set rules required score*/
void set_required_score(ruleset *rules, float required){
   rules->required=required;
}

void set_meta_definition(ruleset *rules, const int ruleno, char *expresion){ 
    meta_definition *def =(meta_definition *)rules->rules[ruleno].def;
    int i=0;
    
    if(def!=NULL){
        if(def->expresion!=NULL) free(def->expresion);
        if(def->dependant_rules!=NULL){
            for(;i<def->dependant_rules->size;i++){
                free(def->dependant_rules->v[i]);
            }
            free(def->dependant_rules);
        }
    }
    
    if( (def=(meta_definition *)malloc(sizeof(meta_definition))) ==NULL){
        wblprintf(LOG_CRITICAL,"RULESET","Not enough memory\n");
        exit(EXIT_FAILURE);
    }
    if( (def->expresion=malloc(sizeof(char)*(strlen(expresion)+1))) ==NULL){
        wblprintf(LOG_CRITICAL,"RULESET","Not enough memory\n");
        exit(EXIT_FAILURE);
    }
    strcpy(def->expresion,expresion);
    //aux->dependant_rules=NULL;
    def->dependant_rules=parse_dependant_rules(expresion);
    def->status=VALID;
    rules->rules[ruleno].def=def;
    
    int *rulepos;
    for(i=0;i<def->dependant_rules->size;i++){
        if( hashmap_get(rules->map,(char *)def->dependant_rules->v[i],(any_t *)&rulepos)==MAP_MISSING &&
            hashmap_get(rules->meta_map,(char *)def->dependant_rules->v[i],(any_t *)&rulepos)==MAP_MISSING )
        {
            wblprintf(LOG_WARNING,"RULESET","Dependant rule '%s' not exists. Ignoring meta rule '%s'\n",
                      (char *)def->dependant_rules->v[i],get_rulename(rules,ruleno));
            
            ((meta_definition *)rules->rules[ruleno].def)->status=INVALID;
            break;
        }
    }
    
    if( ((meta_definition *)rules->rules[ruleno].def)->status!=INVALID ){
        for(i=0;i<def->dependant_rules->size;i++){
            (hashmap_get(rules->dependant_map,(char *)(def->dependant_rules->v[i]),(any_t *)&rulepos)==MAP_MISSING)?
                (hashmap_put(rules->dependant_map,(char *)def->dependant_rules->v[i],rulepos)):
                (0);
        }
        rules->rules[ruleno].def=def;
    }
}

/*
int count_meta_dependant_rules(char *expresion){
    char *start_element=expresion;
    int count=1;
    
    if(strlen(expresion)<=1){
        return 0;
    }
    
    while (start_element[count+1]!='/0'){
        if(start_element[count]=='|' || start_element[count]=='&' ||
           start_element[count]=='+' || start_element[count]=='<' ||
           start_element[count]=='>'){
           count++;
        }    
    }
    return count;
    
}
*/

/*
vector *parse_dependant_rules(char *expresion){
    int num_rules=0;
    vector *aux;
    char *start_element=expresion;
    char *begin=0;
    int count=0;
    int STATUS;
    
    if( (num_rules=count_meta_dependant_rules(expresion)) ==0 ){
        wblprintf(LOG_CRITICAL,"RULESET","Malformed meta expresion\n");
        return NULL;
    }
    
    if( (aux=malloc(sizeof(vector)))==NULL ){
        wblprintf(LOG_CRITICAL,"RULESET","Not enougth memory\n");
        return NULL;
    }
    
    aux->size=num_rules;
    aux->v=malloc(sizeof(char *)*num_rules);
    
    while(start_element[count+1]!='\0'){
        if(begin[])
    
    }
    
    
}
*/

/*Set the rule definition.*/
void set_definition(ruleset *rules, const int ruleno, const char *defName, const char * defParam){ 
    if(((definition *)rules->rules[ruleno].def)!=NULL){
        free(((definition *)rules->rules[ruleno].def)->name);
        if(((definition *)rules->rules[ruleno].def)->param!=NULL)
            (((definition *)rules->rules[ruleno].def)->param);
        if(((definition *)rules->rules[ruleno].def)->pointer!=NULL)
            free(((definition *)rules->rules[ruleno].def)->pointer);
        free(rules->rules[ruleno].def);
    }
    definition *def=(definition *)malloc(sizeof(definition));
    
    //rules->rules[ruleno].def=(definition *)malloc(sizeof(definition));
    def->tflags=NULL;
    def->name=NULL;
    def->pointer=NULL;
    def->param=NULL;
    def->plugin=NULL;
    
    if(def==NULL){
        wblprintf(LOG_CRITICAL,"RULESET","Not enough memory\n");
        exit(EXIT_FAILURE);
    }
    
    def->name=(char *)malloc((strlen(defName)+1)*sizeof(char));
    if(def->name==NULL){
        wblprintf(LOG_CRITICAL,"RULESET","Not enough memory\n");
        exit(EXIT_FAILURE);
    }

    strcpy(def->name,defName);
    //rules->rules[ruleno].def->pointer=NULL;

    if(defParam!=NULL){
        def->param=(char *)malloc((strlen(defParam)+1)*sizeof(char));
        if(def->param==NULL){
            wblprintf(LOG_CRITICAL,"RULESET","Not enough memory\n");
            exit(EXIT_FAILURE);
        }
        strcpy(def->param,defParam);
    }else def->param=NULL;
    
    rules->rules[ruleno].def=def;
}


/*Set the rule name definition.*/
void set_definition_name(ruleset *rules, const int ruleno, const char * newName){
    //primero libero el puntero para no tener basura.
    if(((definition *)rules->rules[ruleno].def)->name!=NULL)
        free(((definition *)rules->rules[ruleno].def)->name);

    ((definition *)rules->rules[ruleno].def)->name=malloc((1+strlen(newName)*sizeof(char)));

    if(((definition *)rules->rules[ruleno].def)->name==NULL){
        wblprintf(LOG_CRITICAL,"RULESET","Not enough memory\n");
        exit(EXIT_FAILURE);
    }
    strcpy(((definition *)rules->rules[ruleno].def)->name,newName);
}

/*Set the rule definition params.*/
void set_definition_param(ruleset *rules, const int ruleno, const char * newParam){
    
    if(((definition *)rules->rules[ruleno].def)->param!=NULL);
        free(((definition *)rules->rules[ruleno].def)->param);
    
    ((definition *)rules->rules[ruleno].def)->param=malloc((1+strlen(newParam)*sizeof(char)));

    if(((definition *)rules->rules[ruleno].def)->param){
        wblprintf(LOG_CRITICAL,"RULESET","Not enough memory\n");
        exit(EXIT_FAILURE);
    }
    strcpy(((definition *)rules->rules[ruleno].def)->param,newParam);
}

/*Set the rule definition pointer*/
void set_definition_pointer(ruleset *rules, const int ruleno, function_t *newPointer){

    if(rules->rules[ruleno].def==NULL){
        wblprintf(LOG_CRITICAL,"RULESET","Rule definition does not exist\n");
        exit(EXIT_FAILURE);
    }
    
    ((definition *)rules->rules[ruleno].def)->pointer=newPointer;
}

/*Set rule tFlags*/
void set_tflags(ruleset *rules, const int ruleno, const char *newFlags){

    if(((definition *)rules->rules[ruleno].def)->tflags!=NULL) free(((definition *)rules->rules[ruleno].def)->tflags);

    ((definition *)rules->rules[ruleno].def)->tflags=malloc((1+strlen(newFlags))*sizeof(char));

    if(((definition *)rules->rules[ruleno].def)->tflags==NULL){
        wblprintf(LOG_CRITICAL,"RULESET","Not enough memory\n");
        exit(EXIT_FAILURE);
    }
    strcpy(((definition *)rules->rules[ruleno].def)->tflags,newFlags);
}

/*
void set_def_parser(ruleset *rules,const int ruleno, const char *parserName ){
    
    if(rules->def_rules[ruleno].par!=NULL){
        if(rules->def_rules[ruleno].par->parserName!=NULL)
            free(rules->def_rules[ruleno].par->parserName);
        if(rules->def_rules[ruleno].par->parserPointer!=NULL)
            free(rules->def_rules[ruleno].par->parserPointer);
    }

    rules->def_rules[ruleno].par=(parser *)malloc(sizeof(parser));

    if(rules->def_rules[ruleno].par==NULL){
        wblprintf(LOG_CRITICAL,"RULESET","Not enough memory\n");
        exit(1);
    }
    
    rules->def_rules[ruleno].par->parserName=(char *)malloc((strlen(parserName)+1)*sizeof(char));

    if(rules->def_rules[ruleno].par->parserName==NULL){
        wblprintf(LOG_CRITICAL,"RULESET","Not enough memory\n");
        exit(1);
    }
    strcpy(rules->def_rules[ruleno].par->parserName,parserName);
    
    rules->def_rules[ruleno].par->parserPointer=NULL;
}
*/

/*Set rule parser*/
void set_parser(ruleset *rules,const int ruleno, const char *parserName){

    if(rules->rules[ruleno].par!=NULL){
        if(rules->rules[ruleno].par->parserName!=NULL)
            free(rules->rules[ruleno].par->parserName);
        if(rules->rules[ruleno].par->parserPointer!=NULL)
            free(rules->rules[ruleno].par->parserPointer);
    }

    rules->rules[ruleno].par=(parser *)malloc(sizeof(parser));

    if(rules->rules[ruleno].par==NULL){
        wblprintf(LOG_CRITICAL,"RULESET","Not enough memory\n");
        exit(EXIT_FAILURE);
    }
    
    rules->rules[ruleno].par->parserName=(char *)malloc((strlen(parserName)+1)*sizeof(char));

    if(rules->rules[ruleno].par->parserName==NULL){
        wblprintf(LOG_CRITICAL,"RULESET","Not enough memory\n");
        exit(EXIT_FAILURE);
    }
    strcpy(rules->rules[ruleno].par->parserName,parserName);
    
    rules->rules[ruleno].par->parserPointer=NULL;
    //rules->rules[ruleno].par->parser_type=parserType;
}



/*Set the rule parser name*/
void set_parser_name(ruleset *rules,const int ruleno, const char *newParserName){

    if(rules->rules[ruleno].par->parserName!=NULL)
        free(rules->rules[ruleno].par->parserName);

    rules->rules[ruleno].par->parserName=malloc((1+strlen(newParserName))*sizeof(char));

    if(rules->rules[ruleno].par->parserName==NULL){
        wblprintf(LOG_CRITICAL,"RULESET","Not enough memory\n");
        exit(EXIT_FAILURE);
    }
    
    strcpy(rules->rules[ruleno].par->parserName,newParserName);
}

/*Set the rule parser pointer*/
void set_parser_pointer(ruleset *rules,int ruleno, parser_t *newParserPointer){
    if(rules->rules[ruleno].par==NULL){
        wblprintf(LOG_CRITICAL,"RULESET","Rule parser does not exist\n");
        exit(EXIT_FAILURE);
    }
    rules->rules[ruleno].par->parserPointer=newParserPointer;
}

/*Set the description of the rule*/
void set_description(ruleset *rules, int ruleno, const char* desc){

    if (rules->rules[ruleno].description!=NULL)
        free(rules->rules[ruleno].description);
    rules->rules[ruleno].description=malloc(sizeof(char)*(strlen(desc)+1));

    if(rules->rules[ruleno].description==NULL){
        wblprintf(LOG_CRITICAL,"RULESET","Not enough memory\n");
        exit(EXIT_FAILURE);
    }
    strcpy(rules->rules[ruleno].description,desc);
}

/*Set the name of the rule*/
void set_rulename(ruleset *rules, int ruleno, const char* rulename){

    if (rules->rules[ruleno].rulename!=NULL) free (rules->rules[ruleno].rulename);

    rules->rules[ruleno].rulename=malloc(sizeof(char)*(strlen(rulename)+1));

    if(rules->rules[ruleno].rulename==NULL){
        wblprintf(LOG_CRITICAL,"RULESET","Not enough memory");
        exit(EXIT_FAILURE);
    }
    strcpy(rules->rules[ruleno].rulename,rulename);
}

/*Get the rule description*/
char *get_description(const ruleset *rules,const int ruleno){
    return rules->rules[ruleno].description;
}

vector *get_dependant_rules(const ruleset *rules, const int ruleno){
    if(((meta_definition *)rules->rules[ruleno].def)==NULL) 
        return NULL;
    else
        return ((meta_definition *)rules->rules[ruleno].def)->dependant_rules;
}

/*Get the rule index in the hash_map*/
int get_rule_index(const ruleset *rules, char* rulename){
   int *ruleno; //The rulenum
   int  error; //Posible error

   //Lookup the rule num
   error=hashmap_get(rules->map, rulename, (any_t *)&ruleno);
   if (error==MAP_MISSING || ruleno==NULL) return -1;

   return *ruleno;
}

int get_meta_index(const ruleset *rules, char* rulename){
   int *ruleno; //The rulenum
   int  error; //Posible error

   //Lookup the rule num
   error=hashmap_get(rules->meta_map, rulename, (any_t *)&ruleno);
   if (error==MAP_MISSING || ruleno==NULL) return -1;

   return *ruleno;
}

void set_debug_mode(int debug){
    debug_mode=debug;
}

/* Update the internal map of a ruleset */
void update_map(ruleset *r){
   int rulenum;
   int *pos;

   //update the map in order to find rules by name
   r->map=hashmap_new();
   for (rulenum=0;rulenum<r->size;rulenum++){
     pos=(int *)malloc(sizeof(int));
     *pos=rulenum;
     hashmap_put(r->map,r->rules[rulenum].rulename,pos);
   }
}

//Set the definition plugin for the rule.
void set_definition_plugin(ruleset *rules, const int ruleno, const char *newPlugin){
    //if(rules->rules[ruleno].def->plugin==NULL) free(rules->rules[ruleno].def->plugin);
    //Como se va a hacer un free de un plugin a NULL??;

    ((definition *)rules->rules[ruleno].def)->plugin=malloc((1+strlen(newPlugin))*sizeof(char));

    if(((definition *)rules->rules[ruleno].def)->plugin==NULL){
        wblprintf(LOG_CRITICAL,"RULESET","Not enough memory\n");
        exit(EXIT_FAILURE);
    }
    strcpy(((definition *)rules->rules[ruleno].def)->plugin,newPlugin);
}

//Retrieves the plugin used to evaluate a rule
char *get_definition_plugin(ruleset *rules, const int ruleno){
    return ((definition *)rules->rules[ruleno].def)->plugin;
}

int is_dependant_rule(ruleset *rules,const int ruleno){
    int *rulepos;
    return hashmap_get(rules->dependant_map,get_rulename(rules,ruleno),(any_t*)&rulepos)!=MAP_MISSING;
}

short is_valid_meta(ruleset *rules, const int ruleno){
    return ( ((meta_definition *)rules->rules[ruleno].def)->status==VALID);
}

int free_vector_element(element data){
    free((char *)data);
    return VECTOR_OK;
}