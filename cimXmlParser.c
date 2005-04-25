
/*
 * cimXmlParser.c
 *
 * (C) Copyright IBM Corp. 2005
 *
 * THIS FILE IS PROVIDED UNDER THE TERMS OF THE COMMON PUBLIC LICENSE
 * ("AGREEMENT"). ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS FILE
 * CONSTITUTES RECIPIENTS ACCEPTANCE OF THE AGREEMENT.
 *
 * You can obtain a current copy of the Common Public License from
 * http://oss.software.ibm.com/developerworks/opensource/license-cpl.html
 *
 * Author:       Adrian Schuur <schuur@de.ibm.com>
 *
 * Description:
 *
 * CIM XML lexer for sfcb to be used in connection with cimXmlOps.y.
 *
*/


#include <stdio.h>
#include <stdlib.h>
#include <cmpidt.h>

#include "cimXmlParser.h"
#include "cimXmlOps.h"
#include "cimXmlRequest.h"
#include "trace.h"


static int attrsOk(XmlBuffer * xb, const XmlElement * e, XmlAttr * r,
                   const char *tag, int etag);
static char *getValue(XmlBuffer * xb, const char *v);
extern int yyparse(void *);

typedef struct tags {
   const char *tag;
   int (*process) (YYSTYPE *, ParserControl * parm);
   int etag;
} Tags;



static void Throw(XmlBuffer * xb, char *msg)
{
   printf("*** Error: %s\n", msg);
   exit(1);
}

static XmlBuffer *newXmlBuffer(char *s)
{
   XmlBuffer *xb = (XmlBuffer *) malloc(sizeof(XmlBuffer));
   xb->base = xb->cur = (char *) strdup(s);
   xb->last = xb->cur + strlen(xb->cur);
   xb->nulledChar = 0;
   xb->eTagFound = 0;
   xb->etag = 0;
   return xb;
}

static void skipWS(XmlBuffer * xb)
{
   static int c = 0;
   c++;
   while (*xb->cur <= ' ' && xb->last > xb->cur)
      xb->cur++;
}

static int getChars(XmlBuffer * xb, const char *s)
{
   int l = strlen(s);
   if (strncmp(xb->cur, s, l) == 0) {
      xb->cur += l;
      return 1;
   }
   return 0;
}

static int getChar(XmlBuffer * xb, const char c)
{
   if (*xb->cur++ == c)
      return *(xb->cur - 1);
   xb->cur--;
   return 0;
}

static char *nextTag(XmlBuffer * xb)
{
   if (xb->nulledChar) {
      xb->nulledChar = 0;
      return xb->cur + 1;
   }
   skipWS(xb);
   if (*xb->cur == '<')
      return xb->cur + 1;
   return NULL;
}

static int nextEquals(const char *n, const char *t)
{
   int l = strlen(t);
   if (strncmp(n, t, l) == 0) {
      if (!isalnum(*(n + l))) {
         return 1;
      }
   }
   return 0;
}

static char skipTag(XmlBuffer * xb)
{
   while (*xb->cur != '>' && xb->last > xb->cur)
      xb->cur++;
   xb->cur++;
   return *xb->cur;
}

/*
static int xmlHdr(XmlBuffer *xb) {
   XmlElement elm[3]={{"version"},{"encoding"},{NULL}};
   XmlAttr vals[2];
   xb->eTagFound = 0;

   skipWS(xb);
   if (getChars(xb,"<?xml")) {
     if (attrsOk(xb,elm,vals,"?xml",ZTOK_CIM)) return 1;
   }
   return 0;
}

static int skipElement(XmlBuffer *xb, const char *t) {
   char eTag[256]="</";
   strcat(eTag,t);
   strcat(eTag,">");
   char *e=strstr(xb->cur,eTag);
   if (e) {
      xb->cur=e+strlen(eTag);
      return 1;
   }
   return 0;
}

static int testAttr(XmlBuffer *xb, char *w, char *val, int xCase) {
   char *end=strchr(xb->cur,'>'),*tcur=xb->cur,dlm,*a=NULL;
   int l=strlen(val),rv=0;

   *end=0;
   a=strstr(xb->cur,w);
   if (a) {
     xb->cur=a+strlen(w);
     skipWS(xb);
     dlm=*(xb->cur);
     if ((xCase && strncmp(xb->cur+1,val,l)==0) || (xCase==0 && strncasecmp(xb->cur+1,val,l)==0)) {
       if (*(xb->cur+l+1)==dlm) rv=1;
     }
   }
   *end='>';
   xb->cur=tcur;
   return rv;
}

static int endTag(XmlBuffer *xb, const char *t, const XmlAttr *a) {
   char *start=xb->cur;
   if (a && xb->eTagFound) return 1;
   skipWS(xb);
   if (getChars(xb,"</")) {
      skipWS(xb);
      if (getWord(xb,t,1)) {
         skipWS(xb);
         if (getChar(xb,'>')) return 1;
      }
   }
   xb->cur=start;
   return 0;
}

*/

static int getWord(XmlBuffer * xb, const char *w, int xCase)
{
   int l = strlen(w);
   if ((xCase && strncmp(xb->cur, w, l) == 0)
       || (xCase == 0 && strncasecmp(xb->cur, w, l) == 0)) {
      if (!isalnum(*(xb->cur + l))) {
         xb->cur += l;
         return 1;
      }
   }
   return 0;
}

static int tagEquals(XmlBuffer * xb, const char *t)
{
   char *start;
   int sz = 0;
   if (*xb->cur == 0) {
      xb->cur++;
      sz = 1;
   }                            // why is this needed ?
   else
      start = xb->cur;
   skipWS(xb);
   if (sz || getChar(xb, '<')) {
      skipWS(xb);
      if (getWord(xb, t, 1))
         return 1;
   }
   else {
      printf("OOOPS\n");
   }
   xb->cur = start;
   return 0;
}

static int attrsOk(XmlBuffer * xb, const XmlElement * e, XmlAttr * r,
                   const char *tag, int etag)
{
   unsigned int n;
   char *ptr, wa[32];
   char msg1[] = { "Unknown attribute in list for " };
   char msg2[] = { "Bad attribute list for " };
   char word[11];

   for (n = 0; (e + n)->attr; n++)
      wa[n] = 0;

   xb->eTagFound = 0;
   for (skipWS(xb); isalpha(*xb->cur); skipWS(xb)) {
//      for (n=0; n < a.size(); n++) {
      for (n = 0; (e + n)->attr; n++) {
         if (wa[n] == 1)
            continue;
         if (getWord(xb, (e + n)->attr, 0)) {
            if (!isalnum(*xb->cur)) {
               skipWS(xb);
               if (getChar(xb, '=')) {
                  (r + n)->attr = getValue(xb, (e + n)->attr);
                  wa[n] = 1;
                  goto ok;
               }
               else
                  Throw(xb, "'=' expected in attribute list");
            }
         }
      }
      strncpy(word, xb->cur, 10);
      word[10] = 0;
      ptr = (char *) alloca(strlen(tag) + strlen(msg1) + 8 + 20);
      strcpy(ptr, msg1);
      strcat(ptr, tag);
      strcat(ptr, " (");
      strcat(ptr, word);
      strcat(ptr, ")");
      Throw(xb, ptr);
    ok:;
   }

   if (getChars(xb, "/>")) {
      xb->eTagFound = 1;
      xb->etag = etag;
      return 1;
   }

   else if (getChar(xb, '>'))
      return 1;

   else if (getChars(xb, "?>") && strcmp(tag, "?xml") == 0) {
      xb->eTagFound = 1;
      xb->etag = etag;
      return 1;
   }

   ptr = (char *) alloca(strlen(tag) + strlen(msg2) + 48);
   strcpy(ptr, msg2);
   strcat(ptr, tag);
   strcat(ptr, ": ");
   strncpy(word, xb->cur, 10);
   strcat(ptr, word);
   strcat(ptr, tag);
   Throw(xb, ptr);
   return -1;
}

/* Is this Broken?  I guess we don't allow escaping the quotes */

static char *getValue(XmlBuffer * xb, const char *v)
{
   skipWS(xb);
   char dlm = 0;
   char *start = NULL;
   if ((dlm = getChar(xb, '"')) || (dlm = getChar(xb, '\''))) {
      start = xb->cur;
      while (*xb->cur != dlm) {
         xb->cur++;
      }
      *xb->cur = 0;
      xb->cur++;
      return start;
   }
   return NULL;
}



static char *getContent(XmlBuffer * xb)
{
   char *start = xb->cur,*end;
   if (xb->eTagFound)
      return NULL;
   for (; *xb->cur != '<' && xb->cur < xb->last; xb->cur++);
   if (start == xb->cur) return "";
//   memmove(start-2,start,xb->cur-start);
   while (*start && *start<=' ') start++;
   xb->nulledChar = *(xb->cur);
   *(xb->cur) = 0;
   end=xb->cur;
   if (*(end-1)<=' ') {
      end--;
      while (*end && *end<=' ') *end--=0;
   }   
   return start;
}



typedef struct types {
   char *str;
   CMPIType type;
} Types;

static Types types[] = {
   {"boolean", CMPI_boolean},
   {"string", CMPI_string},
   {"char16", CMPI_char16},
   {"uint8", CMPI_uint8},
   {"sint8", CMPI_sint8},
   {"uint16", CMPI_uint16},
   {"sint16", CMPI_sint16},
   {"uint32", CMPI_uint32},
   {"sint32", CMPI_sint32},
   {"uint64", CMPI_uint64},
   {"sint64", CMPI_sint64},
   {"datetime", CMPI_dateTime},
   {"real32", CMPI_real32},
   {"real64", CMPI_real64},
   {NULL}
};



//static XmlBuffer* xmb;

static int procCim(YYSTYPE * lvalp, ParserControl * parm)
{
   static XmlElement elm[] = {
      {"CIMVERSION"},
      {"DTDVERSION"},
      {NULL}
   };
   XmlAttr attr[2];

   memset(attr, 0, sizeof(attr));
   if (tagEquals(parm->xmb, "CIM")) {
      if (attrsOk(parm->xmb, elm, attr, "CIM", ZTOK_CIM))
         return XTOK_CIM;
   }
   return 0;
}

static int procMessage(YYSTYPE * lvalp, ParserControl * parm)
{
   static XmlElement elm[] = {
      {"ID"},
      {"PROTOCOLVERSION"},
      {NULL}
   };
   XmlAttr attr[2];

   memset(attr, 0, sizeof(attr));
   if (tagEquals(parm->xmb, "MESSAGE")) {
      if (attrsOk(parm->xmb, elm, attr, "MESSAGE", ZTOK_MESSAGE)) {
         lvalp->xtokMessage.id = attr[0].attr;
         parm->reqHdr.id = attr[0].attr;
         return XTOK_MESSAGE;
      }
   }
   return 0;
}

static int procSimplereq(YYSTYPE * lvalp, ParserControl * parm)
{
   static XmlElement elm[] = {
      {NULL}
   };
   XmlAttr attr[1];
   if (tagEquals(parm->xmb, "SIMPLEREQ")) {
      if (attrsOk(parm->xmb, elm, attr, "SIMPLEREQ", ZTOK_SIMPLEREQ))
         return XTOK_SIMPLEREQ;
   }
   return 0;
}

static int unsupported(ParserControl * parm)
{
   longjmp(parm->env,5);
   return 0;
}

static int procMethodCall(YYSTYPE * lvalp, ParserControl * parm)
{
   static XmlElement elm[] = {
      {"NAME"},
      {NULL}
   };
   XmlAttr attr[1];

   parm->reqHdr.methodCall = 1;
   memset(attr, 0, sizeof(attr));
   if (tagEquals(parm->xmb, "METHODCALL")) {
      if (attrsOk(parm->xmb, elm, attr, "METHODCALL", ZTOK_METHODCALL)) {
         lvalp->xtokMethodCall.method=parm->reqHdr.iMethod = attr[0].attr;
         return XTOK_METHODCALL;
         // return unsupported(parm);
      }   
   }
   return 0;
}

static int procImethodCall(YYSTYPE * lvalp, ParserControl * parm)
{
   static XmlElement elm[] = {
      {"NAME"},
      {NULL}
   };
   XmlAttr attr[1];

   parm->reqHdr.methodCall = 0;
   memset(attr, 0, sizeof(attr));
   if (tagEquals(parm->xmb, "IMETHODCALL")) {
      if (attrsOk(parm->xmb, elm, attr, "IMETHODCALL", ZTOK_IMETHODCALL)) {
         parm->reqHdr.iMethod = attr[0].attr;
         if (strcasecmp(attr[0].attr, "getInstance") == 0)
            return XTOK_GETINSTANCE;
         if (strcasecmp(attr[0].attr, "getClass") == 0)
            return XTOK_GETCLASS;
         if (strcasecmp(attr[0].attr, "enumerateClassNames") == 0)
            return XTOK_ENUMCLASSNAMES;
         if (strcasecmp(attr[0].attr, "enumerateClasses") == 0)
            return XTOK_ENUMCLASSES;
         if (strcasecmp(attr[0].attr, "enumerateInstanceNames") == 0)
            return XTOK_ENUMINSTANCENAMES;
         if (strcasecmp(attr[0].attr, "enumerateInstances") == 0)
            return XTOK_ENUMINSTANCES;
         if (strcasecmp(attr[0].attr, "deleteInstance") == 0)
            return XTOK_DELETEINSTANCE;
         if (strcasecmp(attr[0].attr, "createInstance") == 0)
            return XTOK_CREATEINSTANCE;
         if (strcasecmp(attr[0].attr, "modifyInstance") == 0)
            return XTOK_MODIFYINSTANCE;
         if (strcasecmp(attr[0].attr, "associatorNames") == 0)
            return XTOK_ASSOCIATORNAMES;
         if (strcasecmp(attr[0].attr, "associators") == 0)
            return XTOK_ASSOCIATORS;
         if (strcasecmp(attr[0].attr, "referenceNames") == 0)
            return XTOK_REFERENCENAMES;
         if (strcasecmp(attr[0].attr, "references") == 0)
            return XTOK_REFERENCES;
         if (strcasecmp(attr[0].attr, "execQuery") == 0)
            return XTOK_EXECQUERY;

         if (strcasecmp(attr[0].attr, "createClass") == 0)
            return XTOK_CREATECLASS;
            
         if (strcasecmp(attr[0].attr, "deleteClass") == 0)
            return unsupported(parm);
         if (strcasecmp(attr[0].attr, "getProperty") == 0)
            return unsupported(parm);
         if (strcasecmp(attr[0].attr, "setProperty") == 0)
            return unsupported(parm);
         if (strcasecmp(attr[0].attr, "getQualifier") == 0)
            return unsupported(parm);
         if (strcasecmp(attr[0].attr, "setQualifier") == 0)
            return unsupported(parm);
         if (strcasecmp(attr[0].attr, "deleteQualifier") == 0)
            return unsupported(parm);
         if (strcasecmp(attr[0].attr, "enumerateQualifier") == 0)
            return unsupported(parm);
      }
   }
   return 0;
}

static int procLocalNameSpacePath(YYSTYPE * lvalp, ParserControl * parm)
{
   static XmlElement elm[] = {
      {NULL}
   };
   XmlAttr attr[1];
   if (tagEquals(parm->xmb, "LOCALNAMESPACEPATH")) {
      if (attrsOk
          (parm->xmb, elm, attr, "LOCALNAMESPACEPATH",
           ZTOK_LOCALNAMESPACEPATH)) {
         return XTOK_LOCALNAMESPACEPATH;
      }
   }
   return 0;
}

static int procLocalClassPath(YYSTYPE * lvalp, ParserControl * parm)
{
   static XmlElement elm[] = {
      {NULL}
   };
   XmlAttr attr[1];
   if (tagEquals(parm->xmb, "LOCALCLASSPATH")) {
      if (attrsOk
          (parm->xmb, elm, attr, "LOCALCLASSPATH",
           ZTOK_LOCALCLASSPATH)) {
         return XTOK_LOCALCLASSPATH;
      }
   }
   return 0;
}

static int procLocalInstancePath(YYSTYPE * lvalp, ParserControl * parm)
{
   static XmlElement elm[] = {
      {NULL}
   };
   XmlAttr attr[1];
   if (tagEquals(parm->xmb, "LOCALINSTANCEPATH")) {
      if (attrsOk
          (parm->xmb, elm, attr, "LOCALINSTANCEPATH",
           ZTOK_LOCALINSTANCEPATH)) {
         return XTOK_LOCALINSTANCEPATH;
      }
   }
   return 0;
}

static int procNameSpace(YYSTYPE * lvalp, ParserControl * parm)
{
   static XmlElement elm[] = {
      {"NAME"},
      {NULL}
   };
   XmlAttr attr[1];

   memset(attr, 0, sizeof(attr));
   if (tagEquals(parm->xmb, "NAMESPACE")) {
      if (attrsOk(parm->xmb, elm, attr, "NAMESPACE", ZTOK_NAMESPACE)) {
         lvalp->xtokNameSpace.ns = attr[0].attr;
         return XTOK_NAMESPACE;
      }
   }
   return 0;
}

static int procParamValue(YYSTYPE * lvalp, ParserControl * parm)
{
   static XmlElement elm[] = {
      {"NAME"},
      {"PARAMTYPE"},
      {NULL}
   };
   XmlAttr attr[2];
   int i, m;

   memset(attr, 0, sizeof(attr));
   if (tagEquals(parm->xmb, "PARAMVALUE")) {
      if (attrsOk(parm->xmb, elm, attr, "PARAMVALUE", ZTOK_PARAMVALUE)) {
         lvalp->xtokParamValue.name = attr[0].attr;
         lvalp->xtokParamValue.type = 0;
         if (attr[1].attr) {
            for (i = 0, m = sizeof(types) / sizeof(Types); i < m; i++) {
               if (strcasecmp(attr[1].attr, types[i].str) == 0) {
                  lvalp->xtokParamValue.type = types[i].type;
                  break;
               }
            }
            if (lvalp->xtokParamValue.type==0) {
               if (strcasecmp(attr[1].attr, "reference") == 0) 
                  lvalp->xtokParamValue.type = CMPI_ref;
            }   
         }   
         return XTOK_PARAMVALUE;
      }
   }
   return 0;
}

typedef struct iParm {
   char *name;
   int tag;
} IParm;

static IParm iParms[] = {
   {"classname", XTOK_IP_CLASSNAME},
   {"newinstance", XTOK_IP_INSTANCE},
   {"modifiedinstance", XTOK_IP_MODIFIEDINSTANCE},
   {"instancename", XTOK_IP_INSTANCENAME},
   {"objectname", XTOK_IP_OBJECTNAME},
   {"localonly", XTOK_IP_LOCALONLY},
   {"IncludeQualifiers", XTOK_IP_INCLUDEQUALIFIERS},
   {"IncludeClassOrigin", XTOK_IP_INCLUDECLASSORIGIN},
   {"deepInheritance", XTOK_IP_DEEPINHERITANCE},
   {"assocclass", XTOK_IP_ASSOCCLASS},
   {"resultclass", XTOK_IP_RESULTCLASS},
   {"role", XTOK_IP_ROLE},
   {"resultrole", XTOK_IP_RESULTROLE},
   {"propertylist", XTOK_IP_PROPERTYLIST},
   {"querylanguage", XTOK_IP_QUERYLANG},
   {"query", XTOK_IP_QUERY},
   {"newclass", XTOK_IP_CLASS},
};

static int procIParamValue(YYSTYPE * lvalp, ParserControl * parm)
{
   static XmlElement elm[] = {
      {"NAME"},
      {NULL}
   };
   XmlAttr attr[1];
   int i, m;

   memset(attr, 0, sizeof(attr));
   if (tagEquals(parm->xmb, "IPARAMVALUE")) {
      if (attrsOk(parm->xmb, elm, attr, "IPARAMVALUE", ZTOK_IPARAMVALUE)) {
         for (i = 0, m = sizeof(iParms) / sizeof(IParm); i < m; i++) {
            if (strcasecmp(attr[0].attr, iParms[i].name) == 0) {
               return iParms[i].tag;
            }
         }
      }
   }
   return 0;
}

static int procClassName(YYSTYPE * lvalp, ParserControl * parm)
{
   static XmlElement elm[] = {
      {"NAME"},
      {NULL}
   };
   XmlAttr attr[1];

   memset(attr, 0, sizeof(attr));
   if (tagEquals(parm->xmb, "CLASSNAME")) {
      if (attrsOk(parm->xmb, elm, attr, "CLASSNAME", ZTOK_CLASSNAME)) {
         lvalp->className = attr[0].attr;
         return XTOK_CLASSNAME;
      }
   }
   return 0;
}

static int procInstanceName(YYSTYPE * lvalp, ParserControl * parm)
{
   static XmlElement elm[] = {
      {"CLASSNAME"},
      {NULL}
   };
   XmlAttr attr[1];

   memset(attr, 0, sizeof(attr));
   if (tagEquals(parm->xmb, "INSTANCENAME")) {
      if (attrsOk(parm->xmb, elm, attr, "INSTANCENAME", ZTOK_INSTANCENAME)) {
         lvalp->xtokInstanceName.className = attr[0].attr;
         return XTOK_INSTANCENAME;
      }
   }
   return 0;
}

static int procKeyBinding(YYSTYPE * lvalp, ParserControl * parm)
{
   static XmlElement elm[] = {
      {"NAME"},
      {NULL}
   };
   XmlAttr attr[1];

   memset(attr, 0, sizeof(attr));
   if (tagEquals(parm->xmb, "KEYBINDING")) {
      if (attrsOk(parm->xmb, elm, attr, "KEYBINDING", ZTOK_KEYBINDING)) {
         lvalp->xtokInstanceName.className = attr[0].attr;
         return XTOK_KEYBINDING;
      }
   }
   return 0;
}

static int procInstance(YYSTYPE * lvalp, ParserControl * parm)
{
   static XmlElement elm[] = {
      {"CLASSNAME"},
      {NULL}
   };
   XmlAttr attr[1];

   memset(attr, 0, sizeof(attr));
   if (tagEquals(parm->xmb, "INSTANCE")) {
      if (attrsOk(parm->xmb, elm, attr, "INSTANCE", ZTOK_INSTANCE)) {
         lvalp->xtokInstance.className = attr[0].attr;
         return XTOK_INSTANCE;
      }
   }
   return 0;
}

static int procClass(YYSTYPE * lvalp, ParserControl * parm)
{
   static XmlElement elm[] = {
      {"NAME"},
      {"SUPERCLASS"},
      {NULL}
   };
   XmlAttr attr[2];

   memset(attr, 0, sizeof(attr));
   if (tagEquals(parm->xmb, "CLASS")) {
      if (attrsOk(parm->xmb, elm, attr, "CLASS", ZTOK_CLASS)) {
         lvalp->xtokClass.className = attr[0].attr;
         lvalp->xtokClass.superClass = attr[1].attr;
         return XTOK_CLASS;
      }
   }
   return 0;
}

static int procKeyValue(YYSTYPE * lvalp, ParserControl * parm)
{
   static XmlElement elm[] = {
      {"VALUETYPE"},
      {NULL}
   };
   XmlAttr attr[1] = {
      {NULL}
   };
   char *val;

   memset(attr, 0, sizeof(attr));
   if (tagEquals(parm->xmb, "KEYVALUE")) {
      if (attrsOk(parm->xmb, elm, attr, "KEYVALUE", ZTOK_KEYVALUE)) {
         val = getContent(parm->xmb);
         lvalp->xtokKeyValue.valueType = attr[0].attr;
         lvalp->xtokKeyValue.value = val;
         return XTOK_KEYVALUE;
      }
   }
   return 0;
}

static int procHost(YYSTYPE * lvalp, ParserControl * parm)
{
   static XmlElement elm[] = {
      {NULL}
   };
   XmlAttr attr[1];
   if (tagEquals(parm->xmb, "HOST")) {
      if (attrsOk(parm->xmb, elm, attr, "HOST", ZTOK_HOST)) {
         lvalp->xtokHost.host = getContent(parm->xmb);
         return XTOK_HOST;
      }
   }
   return 0;
}

static int procValue(YYSTYPE * lvalp, ParserControl * parm)
{
   static XmlElement elm[] = {
      {NULL}
   };
   XmlAttr attr[1];
   if (tagEquals(parm->xmb, "VALUE")) {
      char *v;
      if (attrsOk(parm->xmb, elm, attr, "VALUE", ZTOK_VALUE)) {
         v=getContent(parm->xmb);
         lvalp->xtokValue.value = v;
         return XTOK_VALUE;
      }
   }
   return 0;
}

static int procValueArray(YYSTYPE * lvalp, ParserControl * parm)
{
   static XmlElement elm[] = {
      {NULL}
   };
   XmlAttr attr[1];
   if (tagEquals(parm->xmb, "VALUE.ARRAY")) {
      if (attrsOk(parm->xmb, elm, attr, "VALUE.ARRAY",
           ZTOK_VALUEARRAY)) {
         return XTOK_VALUEARRAY;
      }
   }
   return 0;
}

static int procValueNamedInstance(YYSTYPE * lvalp, ParserControl * parm)
{
   static XmlElement elm[] = {
      {NULL}
   };
   XmlAttr attr[1];
   if (tagEquals(parm->xmb, "VALUE.NAMEDINSTANCE")) {
      if (attrsOk(parm->xmb, elm, attr, "VALUE.NAMEDINSTANCE",
           ZTOK_VALUENAMEDINSTANCE)) {
         lvalp->xtokValue.value = getContent(parm->xmb);
         return XTOK_VALUENAMEDINSTANCE;
      }
   }
   return 0;
}

static int procInstancePath(YYSTYPE * lvalp, ParserControl * parm)
{
   static XmlElement elm[] = {
      {NULL}
   };
   XmlAttr attr[1];
   if (tagEquals(parm->xmb, "INSTANCEPATH")) {
      if (attrsOk(parm->xmb, elm, attr, "INSTANCEPATH",
           ZTOK_INSTANCEPATH)) {
         lvalp->xtokValue.value = getContent(parm->xmb);
         return XTOK_INSTANCEPATH;
      }
   }
   return 0;
}

static int procNameSpacePath(YYSTYPE * lvalp, ParserControl * parm)
{
   static XmlElement elm[] = {
      {NULL}
   };
   XmlAttr attr[1];
   if (tagEquals(parm->xmb, "NAMESPACEPATH")) {
      if (attrsOk(parm->xmb, elm, attr, "NAMESPACEPATH",
           ZTOK_NAMESPACEPATH)) {
         lvalp->xtokValue.value = getContent(parm->xmb);
         return XTOK_NAMESPACEPATH;
      }
   }
   return 0;
}

static int procValueReference(YYSTYPE * lvalp, ParserControl * parm)
{
   static XmlElement elm[] = {
      {NULL}
   };
   XmlAttr attr[1];
   if (tagEquals(parm->xmb, "VALUE.REFERENCE")) {
      if (attrsOk(parm->xmb, elm, attr, "VALUE.REFERENCE",
           ZTOK_VALUEREFERENCE)) {
         lvalp->xtokValue.value = getContent(parm->xmb);
         return XTOK_VALUEREFERENCE;
      }
   }
   return 0;
}

static int procXml(YYSTYPE * lvalp, ParserControl * parm)
{
   static XmlElement elm[] = { {"version"},
   {"encoding"},
   {NULL}
   };
   XmlAttr attr[2];

   memset(attr, 0, sizeof(attr));
   if (tagEquals(parm->xmb, "?xml")) {
      if (attrsOk(parm->xmb, elm, attr, "?xml", ZTOK_XML)) {
         return XTOK_XML;
      }
   }
   return 0;
}

static int procQualifier(YYSTYPE * lvalp, ParserControl * parm)
{
   static XmlElement elm[] = { {"NAME"},
   {"TYPE"},
   {"PROPAGATED"},
   {"OVERRIDABLE"},
   {"TOSUBCLASS"},
   {"TOINSTANCE"},
   {"TRANSLATABLE"},
   {NULL}
   };
   XmlAttr attr[8];
   int i, m;

   memset(attr, 0, sizeof(attr));
   if (tagEquals(parm->xmb, "QUALIFIER")) {
      if (attrsOk(parm->xmb, elm, attr, "QUALIFIER", ZTOK_QUALIFIER)) {
         memset(&lvalp->xtokQualifier, 0, sizeof(XtokQualifier));
         lvalp->xtokQualifier.name = attr[0].attr;
         lvalp->xtokQualifier.type = (CMPIType) - 1;
         if (attr[1].attr)
            for (i = 0, m = sizeof(types) / sizeof(Types); i < m; i++) {
               if (strcasecmp(attr[1].attr, types[i].str) == 0) {
                  lvalp->xtokQualifier.type = types[i].type;
                  break;
               }
            }
         if (attr[2].attr)
            lvalp->xtokQualifier.propagated = !strcasecmp(attr[2].attr, "true");
         if (attr[3].attr)
            lvalp->xtokQualifier.overridable =
                !strcasecmp(attr[3].attr, "true");
         if (attr[4].attr)
            lvalp->xtokQualifier.tosubclass = !strcasecmp(attr[4].attr, "true");
         if (attr[5].attr)
            lvalp->xtokQualifier.toinstance = !strcasecmp(attr[5].attr, "true");
         if (attr[6].attr)
            lvalp->xtokQualifier.translatable =
                !strcasecmp(attr[6].attr, "true");
         return XTOK_QUALIFIER;
      }
   }
   return 0;
}


static int procProperty(YYSTYPE * lvalp, ParserControl * parm)
{
   static XmlElement elm[] = { {"NAME"},
   {"TYPE"},
   {"CLASSORIGIN"},
   {"PROPAGATED"},
   {NULL}
   };
   XmlAttr attr[4];
   int i, m;

   memset(attr, 0, sizeof(attr));
   if (tagEquals(parm->xmb, "PROPERTY")) {
      attr[1].attr = NULL;
      lvalp->xtokProperty.valueType = (CMPIType) 1;
      if (attrsOk(parm->xmb, elm, attr, "PROPERTY", ZTOK_PROPERTY)) {
         memset(&lvalp->xtokProperty, 0, sizeof(XtokProperty));
         lvalp->xtokProperty.name = attr[0].attr;
         lvalp->xtokProperty.valueType = (CMPIType) - 1;
         if (attr[1].attr)
            for (i = 0, m = sizeof(types) / sizeof(Types); i < m; i++) {
               if (strcasecmp(attr[1].attr, types[i].str) == 0) {
                  lvalp->xtokProperty.valueType = types[i].type;
                  break;
               }
            }
         lvalp->xtokProperty.classOrigin = attr[2].attr;
         if (attr[3].attr)
            lvalp->xtokProperty.propagated = !strcasecmp(attr[3].attr, "true");
         return XTOK_PROPERTY;
      }
   }
   return 0;
}

static int procPropertyArray(YYSTYPE * lvalp, ParserControl * parm)
{
   static XmlElement elmPA[] = { {"NAME"},
   {"TYPE"},
   {"CLASSORIGIN"},
   {"PROPAGATED"},
   {"ARRAYSIZE"},
   {NULL}
   };
   XmlAttr attr[5];

   memset(attr, 0, sizeof(attr));
   if (tagEquals(parm->xmb, "PROPERTY.ARRAY")) {
      if (attrsOk(parm->xmb, elmPA, attr, "PROPERTY.ARRAY", ZTOK_PROPERTYARRAY)) {
         return XTOK_PROPERTYARRAY;
      }
   }
   return 0;
}

static int procPropertyReference(YYSTYPE * lvalp, ParserControl * parm)
{
   static XmlElement elm[] = { {"NAME"},
   {"REFERENCECLASS"},
   {"CLASSORIGIN"},
   {"PROPAGATED"},
   {NULL}
   };
   XmlAttr attr[4];

   memset(attr, 0, sizeof(attr));
   if (tagEquals(parm->xmb, "PROPERTY.REFERENCE")) {
      attr[1].attr = NULL;
      if (attrsOk(parm->xmb, elm, attr, "PROPERTY.REFERENCE", ZTOK_PROPERTYREFERENCE)) {
         memset(&lvalp->xtokProperty, 0, sizeof(XtokProperty));
         lvalp->xtokProperty.valueType = CMPI_ref;
         lvalp->xtokProperty.name = attr[0].attr;
         lvalp->xtokProperty.referenceClass = attr[1].attr;
         lvalp->xtokProperty.classOrigin = attr[2].attr;
         if (attr[3].attr)
            lvalp->xtokProperty.propagated = !strcasecmp(attr[3].attr, "true");
         return XTOK_PROPERTYREFERENCE;
      }
   }
   return 0;
}

static int procMethod(YYSTYPE * lvalp, ParserControl * parm)
{
   static XmlElement elm[] = { {"NAME"},
   {"TYPE"},
   {"CLASSORIGIN"},
   {"PROPAGATED"},
   {NULL}
   };
   XmlAttr attr[4];
   int i, m;

   memset(attr, 0, sizeof(attr));
   if (tagEquals(parm->xmb, "METHOD")) {
      attr[1].attr = NULL;
      if (attrsOk(parm->xmb, elm, attr, "METHOD", ZTOK_METHOD)) {
         memset(&lvalp->xtokMethod, 0, sizeof(XtokMethod));
         lvalp->xtokMethod.name = attr[0].attr;
         lvalp->xtokMethod.type = CMPI_null;
         if (attr[1].attr)
            for (i = 0, m = sizeof(types) / sizeof(Types); i < m; i++) {
               if (strcasecmp(attr[1].attr, types[i].str) == 0) {
                  lvalp->xtokMethod.type = types[i].type;
                  break;
               }
            }
         lvalp->xtokMethod.classOrigin = attr[2].attr;
         if (attr[3].attr)
            lvalp->xtokMethod.propagated = !strcasecmp(attr[3].attr, "true");
         return XTOK_METHOD;
      }
   }
   return 0;
}

static int procParam(YYSTYPE * lvalp, ParserControl * parm)
{
   static XmlElement elm[] = 
   { {"NAME"},
     {"TYPE"},
     {NULL}
   };
   XmlAttr attr[2];
   int i,m;

   memset(attr, 0, sizeof(attr));
   if (tagEquals(parm->xmb, "PARAMETER")) {
      attr[1].attr = NULL;
      if (attrsOk(parm->xmb, elm, attr, "PARAMETER", ZTOK_PARAM)) {
         memset(&lvalp->xtokParam, 0, sizeof(XtokParam));
         lvalp->xtokParam.pType = ZTOK_PARAM;
         lvalp->xtokParam.name = attr[0].attr;
         lvalp->xtokParam.type = CMPI_null;
         if (attr[1].attr)
            for (i = 0, m = sizeof(types) / sizeof(Types); i < m; i++) {
               if (strcasecmp(attr[1].attr, types[i].str) == 0) {
                  lvalp->xtokParam.type = types[i].type;
                  break;
               }
            }
         return XTOK_PARAM;
      }
   }
   return 0;
}

static int procParamArray(YYSTYPE * lvalp, ParserControl * parm)
{
   static XmlElement elm[] = 
   { {"NAME"},
     {"TYPE"},
     {"ARRAYSIZE"},
     {NULL}
   };
   XmlAttr attr[3];
   int i,m;

   memset(attr, 0, sizeof(attr));
   if (tagEquals(parm->xmb, "PARAMETER.ARRAY")) {
      attr[1].attr = NULL;
      if (attrsOk(parm->xmb, elm, attr, "PARAMETER.ARRAY", ZTOK_PARAM)) {
         memset(&lvalp->xtokParam, 0, sizeof(XtokParam));
         lvalp->xtokParam.pType = ZTOK_PARAMARRAY;
         lvalp->xtokParam.name = attr[0].attr;
         lvalp->xtokParam.type = CMPI_null;
         if (attr[1].attr)
            for (i = 0, m = sizeof(types) / sizeof(Types); i < m; i++) {
               if (strcasecmp(attr[1].attr, types[i].str) == 0) {
                  lvalp->xtokParam.type = types[i].type;
                  lvalp->xtokParam.type |=CMPI_ARRAY;
                  break;
               }
            }
         lvalp->xtokParam.arraySize = attr[2].attr;
         return XTOK_PARAM;
      }
   }
   return 0;
}

static int procParamRef(YYSTYPE * lvalp, ParserControl * parm)
{
   static XmlElement elm[] = 
   { {"NAME"},
     {"REFERENCECLASS"},
     {NULL}
   };
   XmlAttr attr[2];

   memset(attr, 0, sizeof(attr));
   if (tagEquals(parm->xmb, "PARAMETER.REFERENCE")) {
      attr[1].attr = NULL;
      if (attrsOk(parm->xmb, elm, attr, "PARAMETER.REFERENCE", ZTOK_PARAM)) {
         memset(&lvalp->xtokParam, 0, sizeof(XtokParam));
         lvalp->xtokParam.pType = ZTOK_PARAMREF;
         lvalp->xtokParam.name = attr[0].attr;
         lvalp->xtokParam.refClass = attr[1].attr;
         lvalp->xtokParam.type = CMPI_ref;
         return XTOK_PARAM;
      }
   }
   return 0;
}

static int procParamRefArray(YYSTYPE * lvalp, ParserControl * parm)
{
   static XmlElement elm[] = 
   { {"NAME"},
     {"REFERENCECLASS"},
     {"ARRAYSIZE"},
     {NULL}
   };
   XmlAttr attr[3];

   memset(attr, 0, sizeof(attr));
   if (tagEquals(parm->xmb, "PARAMETER.REFARRAY")) {
      attr[1].attr = NULL;
      if (attrsOk(parm->xmb, elm, attr, "PARAMETER.REFARRAY", ZTOK_PARAM)) {
         memset(&lvalp->xtokParam, 0, sizeof(XtokParam));
         lvalp->xtokParam.pType = ZTOK_PARAMREFARRAY;
         lvalp->xtokParam.name = attr[0].attr;
         lvalp->xtokParam.refClass = attr[1].attr;
         lvalp->xtokParam.arraySize = attr[2].attr;
         lvalp->xtokParam.type = CMPI_refA;
         return XTOK_PARAM;
      }
   }
   return 0;
}


static Tags tags[] = {
   {"CIM", procCim, ZTOK_CIM},
   {"MESSAGE", procMessage, ZTOK_MESSAGE},
   {"SIMPLEREQ", procSimplereq, ZTOK_SIMPLEREQ},
   {"IMETHODCALL", procImethodCall, ZTOK_IMETHODCALL},
   {"METHODCALL", procMethodCall, ZTOK_METHODCALL},
   {"LOCALNAMESPACEPATH", procLocalNameSpacePath, ZTOK_LOCALNAMESPACEPATH},
   {"LOCALINSTANCEPATH", procLocalInstancePath, ZTOK_LOCALINSTANCEPATH},
   {"LOCALCLASSPATH", procLocalClassPath, ZTOK_LOCALCLASSPATH},
   {"NAMESPACEPATH", procNameSpacePath, ZTOK_NAMESPACEPATH},
   {"NAMESPACE", procNameSpace, ZTOK_NAMESPACE},
   {"IPARAMVALUE", procIParamValue, ZTOK_IPARAMVALUE},
   {"PARAMVALUE", procParamValue, ZTOK_PARAMVALUE},
   {"CLASSNAME", procClassName, ZTOK_CLASSNAME},
   {"VALUE.ARRAY", procValueArray, ZTOK_VALUEARRAY},
   {"VALUE.NAMEDINSTANCE", procValueNamedInstance, ZTOK_VALUENAMEDINSTANCE},
   {"VALUE.REFERENCE", procValueReference, ZTOK_VALUEREFERENCE},
   {"VALUE", procValue, ZTOK_VALUE},
   {"HOST", procHost, ZTOK_HOST},
   {"KEYVALUE", procKeyValue, ZTOK_KEYVALUE},
   {"KEYBINDING", procKeyBinding, ZTOK_KEYBINDING},
   {"INSTANCEPATH", procInstancePath, ZTOK_INSTANCEPATH},
   {"INSTANCENAME", procInstanceName, ZTOK_INSTANCENAME},
   {"INSTANCE", procInstance, ZTOK_INSTANCE},
   {"PROPERTY.REFERENCE", procPropertyReference, ZTOK_PROPERTYREFERENCE},
   {"PROPERTY.ARRAY", procPropertyArray, ZTOK_PROPERTYARRAY},
   {"PROPERTY", procProperty, ZTOK_PROPERTY},
   {"QUALIFIER", procQualifier, ZTOK_QUALIFIER},
   {"PARAMETER.ARRAY", procParamArray, ZTOK_PARAMARRAY},
   {"PARAMETER.REFERENCE", procParamRef, ZTOK_PARAMREF},
   {"PARAMETER.REFARRAY", procParamRefArray, ZTOK_PARAMREFARRAY},
   {"PARAMETER", procParam, ZTOK_PARAM},
   {"METHOD", procMethod, ZTOK_METHOD},
   {"CLASS", procClass, ZTOK_CLASS},
   {"?xml", procXml, ZTOK_XML},
};

int yylex(YYSTYPE * lvalp, ParserControl * parm)
{
   int i, m, rc;
   char *next;

   _SFCB_ENTER(TRACE_XMLPARSING, "yylex");
   
   for (;;) {
      next = nextTag(parm->xmb);
      if (next == NULL) {
         _SFCB_RETURN(0);
      }   
      _SFCB_TRACE(1, ("--- token: %.32s\n",next));
      if (parm->xmb->eTagFound) {
         parm->xmb->eTagFound = 0;
         _SFCB_RETURN(parm->xmb->etag);
      }

      if (*next == '/') {
         for (i = 0, m = sizeof(tags); i < m; i++) {
            if (nextEquals(next + 1, tags[i].tag) == 1) {
               skipTag(parm->xmb);
               _SFCB_RETURN(tags[i].etag);
            }
         }
      }

      else {
         if (strncmp(parm->xmb->cur, "<!--", 4) == 0) {
            parm->xmb->cur = strstr(parm->xmb->cur, "-->") + 3;
            continue;
         }
         for (i = 0, m = sizeof(tags); i < m; i++) {
            if (nextEquals(next, tags[i].tag) == 1) {
   //printf("+++ %d\n",i);
               rc=tags[i].process(lvalp, parm);
               _SFCB_RETURN(rc);
            }
         }
      }
      break;
   }
   _SFCB_RETURN(0);
}

int yyerror(char *s)
{
   printf("*-* yyerror: %s\n", s);
   exit(5);
}

RequestHdr scanCimXmlRequest(char *xmlData)
{
   ParserControl control;

   XmlBuffer *xmb = newXmlBuffer(xmlData);
   control.xmb = xmb;
   control.reqHdr.xmlBuffer = xmb;
   control.paramValues.last = control.paramValues.first = NULL;
   control.properties.last = control.properties.first = NULL;
   control.qualifiers.last = control.qualifiers.first = NULL;
   control.methods.last = control.methods.first = NULL;
   control.Qs=0;
   control.Ps=0;
   control.Ms=0;
   control.MPs=0;
   control.MQs=0;
   control.MPQs=0;

   if (setjmp(control.env)) {
//      printf("--- setjmp caught !!\n");
      control.reqHdr.opType=0;
      return control.reqHdr;
   }

   control.reqHdr.rc = yyparse(&control);

   return control.reqHdr;
}
