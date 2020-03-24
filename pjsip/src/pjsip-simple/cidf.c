#include <pjsip-simple/cidf.h>
#include <pj/string.h>
#include <pj/pool.h>
#include <pj/assert.h>

static pj_str_t CONFERENCE = { "conference-info", 15 };
static pj_str_t ENTITY = { "entity", 6};
static pj_str_t STATE = { "state", 5};
static pj_str_t VERSION = { "version", 7};
static pj_str_t DESCRIPTION = { "conference-description", 22 };
static pj_str_t SUBJECT = { "subject", 7 };
static pj_str_t SUBJECT_INFO = { "Cd40 Conference", 15 };
static pj_str_t USERS = { "users", 5 };
static pj_str_t USER = { "user", 4 };
static pj_str_t DISPLAY = { "display-text", 12 };
static pj_str_t ROLES = { "roles", 5 };
static pj_str_t ENTRY = { "entry", 5 };
static pj_str_t FULL = { "full", 4 };
static pj_str_t EMPTY_STRING = { NULL, 0 };

static pj_str_t XMLNS = { "xmlns", 5 };
static pj_str_t CIDF_XMLNS = { "urn:ietf:params:xml:ns:conference-info", 38 };

static void xml_init_node(pj_pool_t *pool, pj_xml_node *node, pj_str_t *name, const pj_str_t *value)
{
	pj_list_init(&node->attr_head);
	pj_list_init(&node->node_head);
	node->name = *name;
	if (value) pj_strdup(pool, &node->content, value);
	else node->content.ptr=NULL, node->content.slen=0;
}

static pj_xml_attr* xml_create_attr(pj_pool_t *pool, pj_str_t *name, const pj_str_t *value)
{
	pj_xml_attr *attr = PJ_POOL_ALLOC_T(pool, pj_xml_attr);
	attr->name = *name;
	pj_strdup(pool, &attr->value, value);
	return attr;
}

/* Conference */
PJ_DEF(void) pjcidf_conf_construct(pj_pool_t *pool, pjcidf_conf *conf, const pj_str_t *entity, unsigned version, const pj_str_t *state)
{
	pj_xml_attr *attr;
	char buff[25];
	pj_str_t ver;
	char centity1[256];
	pj_str_t entity1;
	int i = 0;

	centity1[0] = '\0';	

	xml_init_node(pool, conf, &CONFERENCE, NULL);
	attr = xml_create_attr(pool, &XMLNS, &CIDF_XMLNS);
	pj_xml_add_attr(conf, attr);

	//De entity me quedo solo con la URI y le quito los parámetros
	i = 0;
	while(entity->ptr[i] == ' ' || entity->ptr[i] == '<')
	{
		i++;
		if (i == entity->slen)
		{
			break;
		}
	}

	if (i == entity->slen)
	{
		//entity solo tiene espacios o '<'. Seria raro. No hacemos nada lo dejamos todo como esta.
	}
	else
	{
		strncpy(centity1, &entity->ptr[i], sizeof(centity1));
		centity1[sizeof(centity1)-1] = '\0';
		//Truco a partir del primer ';' para eliminar los parametros de la uri. Si los hubiera.
		for (i = 0; i < sizeof(centity1); i++)
		{
			if (centity1[i] == ';')
			{
				centity1[i] = '\0';
				break;
			}
		}
	}

	pj_strset2(&entity1, centity1);

	//attr = xml_create_attr(pool, &ENTITY, entity);
	attr = xml_create_attr(pool, &ENTITY, &entity1);
	pj_xml_add_attr(conf, attr);

	if (state && state->slen)
	{
		attr = xml_create_attr(pool, &STATE, state);
		pj_xml_add_attr(conf, attr);
	}

	pj_utoa(version, buff);
	ver = pj_str(buff);

	attr = xml_create_attr(pool, &VERSION, &ver);
	pj_xml_add_attr(conf, attr);

	if (!state || (pj_stricmp2(state, "deleted") != 0))
	{
		pj_xml_node *description = PJ_POOL_ALLOC_T(pool, pj_xml_node);
		//pj_xml_node *subject = PJ_POOL_ALLOC_T(pool, pj_xml_node);

		xml_init_node(pool, description, &DESCRIPTION, NULL);
		pj_xml_add_node(conf, description);

		//Quito el emvio de subject para que el paquete sea mas pequeño y parece que no se usa
		//xml_init_node(pool, subject, &SUBJECT, &SUBJECT_INFO);
		//pj_xml_add_node(description, subject);
	}
}

PJ_DEF(unsigned) pjcidf_conf_get_version(const pjcidf_conf *conf )
{
	const pj_xml_attr *attr = pj_xml_find_attr(conf, &VERSION, NULL);
	if (attr)
		return pj_strtoul(&attr->value);

	return 0;
}

PJ_DEF(const pj_str_t*) pjcidf_conf_get_state(const pjcidf_conf *conf )
{
	const pj_xml_attr *attr = pj_xml_find_attr(conf, &STATE, NULL);
	if (attr)
		return &attr->value;

	return &FULL;
}

PJ_DEF(pjcidf_user*) pjcidf_conf_add_user(pj_pool_t *pool, pjcidf_conf *conf, const pj_str_t *id, const pj_str_t *display, const pj_str_t *role)
{
	pjcidf_user *u;
	pj_xml_node *users = pj_xml_find_node(conf, &USERS);

	if (!users) {
		users = PJ_POOL_ALLOC_T(pool, pj_xml_node);
		xml_init_node(pool, users, &USERS, NULL);
		pj_xml_add_node(conf, users);
	}

	u = PJ_POOL_ALLOC_T(pool, pjcidf_user);
	pjcidf_user_construct(pool, u, id, display, role);
	pj_xml_add_node(users, u);

	return u;
}

PJ_DEF(pjcidf_user*) pjcidf_conf_get_first_user(pjcidf_conf *conf)
{
	pj_xml_node *users = pj_xml_find_node(conf, &USERS);
	if (users)
		return pj_xml_find_node(users, &USER);

	return NULL;
}

PJ_DEF(pjcidf_user*) pjcidf_conf_get_next_user(pjcidf_conf *conf, pjcidf_user *u)
{
	pj_xml_node *users = pj_xml_find_node(conf, &USERS);
	return pj_xml_find_next_node(users, u, &USER);
}

/* User */
PJ_DEF(void) pjcidf_user_construct(pj_pool_t *pool, pjcidf_user *u, const pj_str_t *id, const pj_str_t *display, const pj_str_t *role)
{
	pj_xml_attr *attr;

	xml_init_node(pool, u, &USER, NULL);
	attr = xml_create_attr(pool, &ENTITY, id);
	pj_xml_add_attr(u, attr);

	if (display && display->slen)
	{
		pj_xml_node *node = PJ_POOL_ALLOC_T(pool, pj_xml_node);
		xml_init_node(pool, node, &DISPLAY, display);
		pj_xml_add_node(u, node);
	}
	if (role && role->slen)
	{
		pj_xml_node *roles = PJ_POOL_ALLOC_T(pool, pj_xml_node);
		pj_xml_node *entry = PJ_POOL_ALLOC_T(pool, pj_xml_node);

		xml_init_node(pool, roles, &ROLES, NULL);
		pj_xml_add_node(u, roles);

		xml_init_node(pool, entry, &ENTRY, role);
		pj_xml_add_node(roles, entry);
	}
}

PJ_DEF(const pj_str_t*) pjcidf_user_get_id(const pjcidf_user *u )
{
	const pj_xml_attr *attr = pj_xml_find_attr(u, &ENTITY, NULL);
	pj_assert(attr);
	return &attr->value;
}

PJ_DEF(const pj_str_t*) pjcidf_user_get_display(const pjcidf_user *u)
{
	pj_xml_node *node = pj_xml_find_node(u, &DISPLAY);
	if (!node)
		return &EMPTY_STRING;
	return &node->content;
}

PJ_DEF(const pj_str_t*) pjcidf_user_get_role(const pjcidf_user *u)
{
	pj_xml_node *roles = pj_xml_find_node(u, &ROLES);
	if (roles)
	{
		pj_xml_node *entry = pj_xml_find_node(roles, &ENTRY);
		if (entry)
			return &entry->content;
	}

	return &EMPTY_STRING;
}

PJ_DEF(const pj_str_t*) pjcidf_user_get_state(const pjcidf_user *u )
{
	const pj_xml_attr *attr = pj_xml_find_attr(u, &STATE, NULL);
	if (attr)
		return &attr->value;

	return &FULL;
}

PJ_DEF(pjcidf_conf*) pjcidf_create(pj_pool_t *pool, const pj_str_t *entity, unsigned version, const pj_str_t *state)
{
	pjcidf_conf *conf = PJ_POOL_ALLOC_T(pool, pjcidf_conf);
	pjcidf_conf_construct(pool, conf, entity, version, state);
	return conf;
}

PJ_DEF(pjcidf_conf*) pjcidf_parse(pj_pool_t *pool, char *text, int len)
{
	pjcidf_conf *conf = pj_xml_parse(pool, text, len);
	if (conf && conf->name.slen >= CONFERENCE.slen) {
		pj_str_t name;

		name.ptr = conf->name.ptr;
		name.slen = CONFERENCE.slen;

		if (pj_stricmp(&name, &CONFERENCE) == 0)
			return conf;
	}

	return NULL;
}

