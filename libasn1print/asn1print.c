#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <asn1parser.h>
#include <asn1fix_export.h>
#include <asn1fix_crange.h>

#include "asn1print.h"

#define	INDENT(fmt, args...)	do {					\
		if(!(flags & APF_NOINDENT)) {				\
			int __i = level;				\
			while(__i--) printf("    ");			\
		}							\
		printf(fmt, ##args);					\
	} while(0)

static int asn1print_module(asn1p_t *asn, asn1p_module_t *mod, enum asn1print_flags flags);
static int asn1print_oid(int prior_len, asn1p_oid_t *oid, enum asn1print_flags flags);
static int asn1print_ref(asn1p_ref_t *ref, enum asn1print_flags flags);
static int asn1print_tag(asn1p_expr_t *tc, enum asn1print_flags flags);
static int asn1print_params(asn1p_paramlist_t *pl,enum asn1print_flags flags);
static int asn1print_with_syntax(asn1p_wsyntx_t *wx, enum asn1print_flags flags);
static int asn1print_constraint(asn1p_constraint_t *, enum asn1print_flags);
static int asn1print_value(asn1p_value_t *val, enum asn1print_flags flags);
static int asn1print_expr(asn1p_t *asn, asn1p_module_t *mod, asn1p_expr_t *tc, enum asn1print_flags flags, int level);
static int asn1print_expr_dtd(asn1p_t *asn, asn1p_module_t *mod, asn1p_expr_t *tc, enum asn1print_flags flags, int level);

/*
 * Print the contents of the parsed ASN tree.
 */
int
asn1print(asn1p_t *asn, enum asn1print_flags flags) {
	asn1p_module_t *mod;
	int modno = 0;

	if(asn == NULL) {
		errno = EINVAL;
		return -1;
	}

	if(flags & APF_PRINT_XML_DTD)
		printf("<!-- XML DTD generated by asn1c-" VERSION " -->\n\n");

	TQ_FOR(mod, &(asn->modules), mod_next) {
		if(mod->_tags & MT_STANDARD_MODULE)
			return 0; /* Ignore modules imported from skeletons */
		if(modno++) printf("\n");
		asn1print_module(asn, mod, flags);
	}

	if(flags & APF_PRINT_XML_DTD) {
		/* Values for BOOLEAN */
		printf("<!ELEMENT true EMPTY>\n");
		printf("<!ELEMENT false EMPTY>\n");
	}

	return 0;
}

static int
asn1print_module(asn1p_t *asn, asn1p_module_t *mod, enum asn1print_flags flags) {
	asn1p_expr_t *tc;

	if(flags & APF_PRINT_XML_DTD)
		printf("<!-- ASN.1 module\n");

	printf("%s ", mod->ModuleName);
	if(mod->module_oid) {
		asn1print_oid(strlen(mod->ModuleName), mod->module_oid, flags);
		printf("\n");
	}

	if(flags & APF_PRINT_XML_DTD) {
		if(mod->source_file_name
		&& strcmp(mod->source_file_name, "-"))
			printf("found in %s", mod->source_file_name);
		printf(" -->\n\n");

		TQ_FOR(tc, &(mod->members), next) {
			asn1print_expr_dtd(asn, mod, tc, flags, 0);
		}

		return 0;
	}

	printf("DEFINITIONS");

	if(mod->module_flags & MSF_TAG_INSTRUCTIONS)
		printf(" TAG INSTRUCTIONS");
	if(mod->module_flags & MSF_XER_INSTRUCTIONS)
		printf(" XER INSTRUCTIONS");
	if(mod->module_flags & MSF_EXPLICIT_TAGS)
		printf(" EXPLICIT TAGS");
	if(mod->module_flags & MSF_IMPLICIT_TAGS)
		printf(" IMPLICIT TAGS");
	if(mod->module_flags & MSF_AUTOMATIC_TAGS)
		printf(" AUTOMATIC TAGS");
	if(mod->module_flags & MSF_EXTENSIBILITY_IMPLIED)
		printf(" EXTENSIBILITY IMPLIED");

	printf(" ::=\n");
	printf("BEGIN\n\n");

	TQ_FOR(tc, &(mod->members), next) {
		asn1print_expr(asn, mod, tc, flags, 0);
		if(flags & APF_PRINT_CONSTRAINTS)
			printf("\n");
		else
			printf("\n\n");
	}

	printf("END\n");

	return 0;
}

static int
asn1print_oid(int prior_len, asn1p_oid_t *oid, enum asn1print_flags flags) {
	size_t accum = prior_len;
	int ac;

	(void)flags;	/* Unused argument */

	printf("{");
	for(ac = 0; ac < oid->arcs_count; ac++) {
		const char *arcname = oid->arcs[ac].name;

		if(accum + strlen(arcname ? arcname : "") > 72) {
			printf("\n\t");
			accum = 8;
		} else {
			accum += printf(" ");
		}

		if(arcname) {
			accum += printf("%s", arcname);
			if(oid->arcs[ac].number >= 0) {
				accum += printf("(%" PRIdASN ")",
					oid->arcs[ac].number);
			}
		} else {
			accum += printf("%" PRIdASN, oid->arcs[ac].number);
		}
	}
	printf(" }");

	return 0;
}

static int
asn1print_ref(asn1p_ref_t *ref, enum asn1print_flags flags) {
	int cc;

	(void)flags;	/* Unused argument */

	for(cc = 0; cc < ref->comp_count; cc++) {
		if(cc) printf(".");
		printf("%s", ref->components[cc].name);
	}

	return 0;
}

static int
asn1print_tag(asn1p_expr_t *tc, enum asn1print_flags flags) {
	struct asn1p_type_tag_s *tag = &tc->tag;

	(void)flags;	/* Unused argument */

	printf("%s", asn1p_tag2string(tag, 0));

	return 0;
}

static int
asn1print_value(asn1p_value_t *val, enum asn1print_flags flags) {

	if(val == NULL)
		return 0;

	switch(val->type) {
	case ATV_NOVALUE:
		break;
	case ATV_NULL:
		printf("NULL");
		return 0;
	case ATV_REAL:
		printf("%f", val->value.v_double);
		return 0;
	case ATV_INTEGER:
		printf("%" PRIdASN, val->value.v_integer);
		return 0;
	case ATV_MIN: printf("MIN"); return 0;
	case ATV_MAX: printf("MAX"); return 0;
	case ATV_FALSE: printf("FALSE"); return 0;
	case ATV_TRUE: printf("TRUE"); return 0;
	case ATV_TUPLE:
		printf("{%d, %d}",
			(int)(val->value.v_integer >> 4),
			(int)(val->value.v_integer & 0x0f));
		return 0;
	case ATV_QUADRUPLE:
		printf("{%d, %d, %d, %d}",
			(int)((val->value.v_integer >> 24) & 0xff),
			(int)((val->value.v_integer >> 16) & 0xff),
			(int)((val->value.v_integer >> 8) & 0xff),
			(int)((val->value.v_integer >> 0) & 0xff)
		);
		return 0;
	case ATV_STRING:
		{
			char *p = (char *)val->value.string.buf;
			putchar('"');
			if(strchr(p, '"')) {
				/* Mask quotes */
				for(; *p; p++) {
					if(*p == '"')
						putchar(*p);
					putchar(*p);
				}
			} else {
				fputs(p, stdout);
			}
			putchar('"');
		}
		return 0;
	case ATV_UNPARSED:
		fputs((char *)val->value.string.buf, stdout);
		return 0;
	case ATV_BITVECTOR:
		{
			uint8_t *bitvector;
			int bits;
			int i;

			bitvector = val->value.binary_vector.bits;
			bits = val->value.binary_vector.size_in_bits;

			printf("'");
			if(bits%8) {
				for(i = 0; i < bits; i++) {
					uint8_t uc;
					uc = bitvector[i>>3];
					putchar(((uc >> (7-(i%8)))&1)?'1':'0');
				}
				printf("'B");
			} else {
				char hextable[16] = "0123456789ABCDEF";
				for(i = 0; i < (bits>>3); i++) {
					putchar(hextable[bitvector[i] >> 4]);
					putchar(hextable[bitvector[i] & 0x0f]);
				}
				printf("'H");
			}
			return 0;
		}
	case ATV_REFERENCED:
		return asn1print_ref(val->value.reference, flags);
	case ATV_VALUESET:
		return asn1print_constraint(val->value.constraint, flags);
	case ATV_CHOICE_IDENTIFIER:
		printf("%s: ", val->value.choice_identifier.identifier);
		return asn1print_value(val->value.choice_identifier.value, flags);
	}

	assert(val->type || !"Unknown");

	return 0;
}

static int
asn1print_constraint(asn1p_constraint_t *ct, enum asn1print_flags flags) {
	int symno = 0;

	if(ct == 0) return 0;

	if(ct->type == ACT_CA_SET)
		printf("(");

	switch(ct->type) {
	case ACT_EL_TYPE:
		asn1print_value(ct->containedSubtype, flags);
		break;
	case ACT_EL_VALUE:
		asn1print_value(ct->value, flags);
		break;
	case ACT_EL_RANGE:
	case ACT_EL_LLRANGE:
	case ACT_EL_RLRANGE:
	case ACT_EL_ULRANGE:
		asn1print_value(ct->range_start, flags);
			switch(ct->type) {
			case ACT_EL_RANGE: printf(".."); break;
			case ACT_EL_LLRANGE: printf("<.."); break;
			case ACT_EL_RLRANGE: printf("..<"); break;
			case ACT_EL_ULRANGE: printf("<..<"); break;
			default: printf("?..?"); break;
			}
		asn1print_value(ct->range_stop, flags);
		break;
	case ACT_EL_EXT:
		printf("...");
		break;
	case ACT_CT_SIZE:
	case ACT_CT_FROM:
		switch(ct->type) {
		case ACT_CT_SIZE: printf("SIZE("); break;
		case ACT_CT_FROM: printf("FROM("); break;
		default: printf("??? ("); break;
		}
		assert(ct->el_count != 0);
		assert(ct->el_count == 1);
		asn1print_constraint(ct->elements[0], flags);
		printf(")");
		break;
	case ACT_CT_WCOMP:
		assert(ct->el_count != 0);
		assert(ct->el_count == 1);
		printf("WITH COMPONENT (");
		asn1print_constraint(ct->elements[0], flags);
		printf(")");
		break;
	case ACT_CT_WCOMPS: {
			unsigned int i;
			printf("WITH COMPONENTS { ");
			for(i = 0; i < ct->el_count; i++) {
				asn1p_constraint_t *cel = ct->elements[i];
				if(i) printf(", ");
				fwrite(cel->value->value.string.buf,
					1, cel->value->value.string.size,
					stdout);
				if(cel->el_count) {
					assert(cel->el_count == 1);
					printf(" ");
					asn1print_constraint(cel->elements[0],
						flags);
				}
				switch(cel->presence) {
				case ACPRES_DEFAULT: break;
				case ACPRES_PRESENT: printf(" PRESENT"); break;
				case ACPRES_ABSENT: printf(" ABSENT"); break;
				case ACPRES_OPTIONAL: printf(" OPTIONAL");break;
				}
			}
			printf(" }");
		}
		break;
	case ACT_CT_CTDBY:
		printf("CONSTRAINED BY ");
		assert(ct->value->type == ATV_UNPARSED);
		fwrite(ct->value->value.string.buf,
			1, ct->value->value.string.size, stdout);
		break;
	case ACT_CA_SET: symno++;
	case ACT_CA_CRC: symno++;
	case ACT_CA_CSV: symno++;
	case ACT_CA_UNI: symno++;
	case ACT_CA_INT: symno++;
	case ACT_CA_EXC:
		{
			char *symtable[] = { " EXCEPT ", " ^ ", " | ", ",",
					"", "(" };
			unsigned int i;
			for(i = 0; i < ct->el_count; i++) {
				if(i) fputs(symtable[symno], stdout);
				if(ct->type == ACT_CA_CRC) fputs("{", stdout);
				asn1print_constraint(ct->elements[i], flags);
				if(ct->type == ACT_CA_CRC) fputs("}", stdout);
				if(i+1 < ct->el_count
				&& ct->type == ACT_CA_SET)
					fputs(")", stdout);
			}
		}
		break;
	case ACT_CA_AEX:
		assert(ct->el_count == 1);
		printf("ALL EXCEPT ");
		asn1print_constraint(ct->elements[0], flags);
		break;
	case ACT_INVALID:
		assert(ct->type != ACT_INVALID);
		break;
	}

	if(ct->type == ACT_CA_SET)
		printf(")");

	return 0;
}

static int
asn1print_params(asn1p_paramlist_t *pl, enum asn1print_flags flags) {
	if(pl) {
		int i;
		printf("{");
		for(i = 0; i < pl->params_count; i++) {
			if(i) printf(", ");
			if(pl->params[i].governor) {
				asn1print_ref(pl->params[i].governor, flags);
				printf(":");
			}
			printf("%s", pl->params[i].argument);
		}
		printf("}");
	}

	return 0;
}

static int
asn1print_with_syntax(asn1p_wsyntx_t *wx, enum asn1print_flags flags) {
	if(wx) {
		asn1p_wsyntx_chunk_t *wc;
		TQ_FOR(wc, &(wx->chunks), next) {
		  switch(wc->type) {
		  case WC_LITERAL:
		  case WC_WHITESPACE:
		  case WC_FIELD:
			printf("%s", wc->content.token);
			break;
		  case WC_OPTIONALGROUP:
			printf("[");
			asn1print_with_syntax(wc->content.syntax,flags);
			printf("]");
			break;
		  }
		}
	}

	return 0;
}

static int
asn1print_crange_value(asn1cnst_edge_t *edge, int as_char) {
	switch(edge->type) {
	case ARE_MIN: printf("MIN"); break;
	case ARE_MAX: printf("MAX"); break;
	case ARE_VALUE:
		if(as_char) {
			printf("\"%c\"", (unsigned char)edge->value);
		} else {
			printf("%" PRIdASN, edge->value);
		}
	}
	return 0;
}

static int
asn1print_constraint_explain_type(asn1p_expr_type_e expr_type, asn1p_constraint_t *ct, enum asn1p_constraint_type_e type, int strict_PER_visible) {
	asn1cnst_range_t *range;
	int as_char = (type==ACT_CT_FROM);
	int i;

	range = asn1constraint_compute_PER_range(expr_type, ct, type, 0, 0,
			strict_PER_visible ? CPR_strict_PER_visibility : 0);
	if(!range) return -1;

	if(range->incompatible
	|| (strict_PER_visible && range->not_PER_visible)) {
		asn1constraint_range_free(range);
		return 0;
	}

	switch(type) {
	case ACT_CT_FROM: printf("(FROM("); break;
	case ACT_CT_SIZE: printf("(SIZE("); break;
	default: printf("("); break;
	}
	for(i = -1; i < range->el_count; i++) {
		asn1cnst_range_t *r;
		if(i == -1) {
			if(range->el_count) continue;
			r = range;
		} else {
			r = range->elements[i];
		}
		if(i > 0) {
			printf(" | ");
		}
		asn1print_crange_value(&r->left, as_char);
		if(r->left.type != r->right.type
		|| r->left.value != r->right.value) {
			printf("..");
			asn1print_crange_value(&r->right, as_char);
		}
	}
	if(range->extensible)
		printf(",...");
	printf(type==ACT_EL_RANGE?")":"))");

	if(range->empty_constraint)
		printf(":Empty!");

	asn1constraint_range_free(range);
	return 0;
}

static int
asn1print_constraint_explain(asn1p_expr_type_e expr_type,
		asn1p_constraint_t *ct, int s_PV) {

	asn1print_constraint_explain_type(expr_type, ct, ACT_EL_RANGE, s_PV);
	printf(" ");
	asn1print_constraint_explain_type(expr_type, ct, ACT_CT_SIZE, s_PV);
	printf(" ");
	asn1print_constraint_explain_type(expr_type, ct, ACT_CT_FROM, s_PV);

	return 0;
}

static int
asn1print_expr(asn1p_t *asn, asn1p_module_t *mod, asn1p_expr_t *tc, enum asn1print_flags flags, int level) {
	int SEQ_OF = 0;

	if(flags & APF_LINE_COMMENTS && !(flags & APF_NOINDENT))
		INDENT("-- #line %d\n", tc->_lineno);

	/* Reconstruct compiler directive information */
	if((tc->marker.flags & EM_INDIRECT)
	&& (tc->marker.flags & EM_OMITABLE) != EM_OMITABLE) {
		if((flags & APF_NOINDENT))
			printf(" --<ASN1C.RepresentAsPointer>-- ");
		else
			INDENT("--<ASN1C.RepresentAsPointer>--\n");
	}

	if(tc->Identifier
	&& (!(tc->meta_type == AMT_VALUE && tc->expr_type == A1TC_REFERENCE)
	 || level == 0))
		INDENT("%s", tc->Identifier);

	if(tc->lhs_params) {
		asn1print_params(tc->lhs_params, flags);
	}

	if(tc->meta_type != AMT_VALUE
	&& tc->meta_type != AMT_VALUESET
	&& tc->expr_type != A1TC_EXTENSIBLE) {
		if(level) {
			if(tc->Identifier && !(flags & APF_NOINDENT))
				printf("\t");
		} else {
			printf(" ::=");
		}
	}

	if(tc->tag.tag_class) {
		printf(" ");
		asn1print_tag(tc, flags);
	}

	switch(tc->expr_type) {
	case A1TC_EXTENSIBLE:
		if(tc->value) {
			printf("!");
			asn1print_value(tc->value, flags);
		}
		break;
	case A1TC_COMPONENTS_OF:
		SEQ_OF = 1; /* Equivalent to SET OF for printint purposes */
		printf("    COMPONENTS OF");
		break;
	case A1TC_REFERENCE:
	case A1TC_UNIVERVAL:
		break;
	case A1TC_CLASSDEF:
		printf(" CLASS");
		break;
	case A1TC_CLASSFIELD_TFS ... A1TC_CLASSFIELD_OSFS:
		/* Nothing to print here */
		break;
	case ASN_CONSTR_SET_OF:
	case ASN_CONSTR_SEQUENCE_OF:
		SEQ_OF = 1;
		if(tc->expr_type == ASN_CONSTR_SET_OF)
			printf(" SET");
		else
			printf(" SEQUENCE");
		if(tc->constraints) {
			printf(" ");
			asn1print_constraint(tc->constraints, flags);
		}
		printf(" OF");
		break;
	case A1TC_VALUESET:
		break;
	default:
		{
			char *p = ASN_EXPR_TYPE2STR(tc->expr_type);
			printf(" %s", p?p:"<unknown type!>");
		}
		break;
	}

	/*
	 * Put the name of the referred type.
	 */
	if(tc->reference) {
		printf(" ");
		asn1print_ref(tc->reference, flags);
	}

	if(tc->meta_type == AMT_VALUESET && level == 0)
		printf(" ::=");

	/*
	 * Display the descendants (children) of the current type.
	 */
	if(TQ_FIRST(&(tc->members))
	|| (tc->expr_type & ASN_CONSTR_MASK)
	|| tc->meta_type == AMT_OBJECT
	|| tc->meta_type == AMT_OBJECTCLASS
	|| tc->meta_type == AMT_OBJECTFIELD
	) {
		asn1p_expr_t *se;	/* SubExpression */
		int put_braces = (!SEQ_OF) /* Don't need 'em, if SET OF... */
			&& (tc->meta_type != AMT_OBJECTFIELD);

		if(put_braces) {
			if(flags & APF_NOINDENT) {
				printf("{");
				if(!TQ_FIRST(&tc->members))
					printf("}");
			} else {
				printf(" {");
				if(TQ_FIRST(&tc->members))
					printf("\n");
				else
					printf(" }");
			}
		}

		TQ_FOR(se, &(tc->members), next) {
			/*
			 * Print the expression as it were a stand-alone type.
			 */
			asn1print_expr(asn, mod, se, flags, level + 1);
			if((se->marker.flags & EM_DEFAULT) == EM_DEFAULT) {
				printf(" DEFAULT ");
				asn1print_value(se->marker.default_value, flags);
			} else if((se->marker.flags & EM_OPTIONAL)
					== EM_OPTIONAL) {
				printf(" OPTIONAL");
			}
			if(TQ_NEXT(se, next)) {
				printf(",");
				if(!(flags & APF_NOINDENT))
					INDENT("\n");
			}
		}

		if(put_braces && TQ_FIRST(&tc->members)) {
			if(!(flags & APF_NOINDENT))
				printf("\n");
			INDENT("}");
		}
	}

	if(tc->with_syntax) {
		printf(" WITH SYNTAX {");
		asn1print_with_syntax(tc->with_syntax, flags);
		printf("}\n");
	}

	/* Right hand specialization */
	if(tc->rhs_pspecs) {
		asn1p_expr_t *se;
		printf("{");
		TQ_FOR(se, &(tc->rhs_pspecs->members), next) {
			asn1print_expr(asn, mod, se, flags, level + 1);
			if(TQ_NEXT(se, next)) printf(", ");
		}
		printf("}");
	}

	if(!SEQ_OF && tc->constraints) {
		printf(" ");
		if(tc->meta_type == AMT_VALUESET)
			printf("{");
		asn1print_constraint(tc->constraints, flags);
		if(tc->meta_type == AMT_VALUESET)
			printf("}");
	}

	if(tc->unique) {
		printf(" UNIQUE");
	}

	if(tc->meta_type == AMT_VALUE
	&& tc->expr_type != A1TC_EXTENSIBLE) {
		if(tc->expr_type == A1TC_UNIVERVAL) {
			if(tc->value) {
				printf("(");
				asn1print_value(tc->value, flags);
				printf(")");
			}
		} else {
			if(level == 0) printf(" ::= ");
			asn1print_value(tc->value, flags);
		}
	}

	/*
	 * The following section exists entirely for debugging.
	 */
	if(flags & APF_PRINT_CONSTRAINTS
	&& tc->expr_type != A1TC_EXTENSIBLE) {
		asn1p_expr_t *top_parent;

		if(tc->combined_constraints) {
			printf("\n-- Combined constraints: ");
			asn1print_constraint(tc->combined_constraints, flags);
		}

		top_parent = asn1f_find_terminal_type_ex(asn, tc);
		if(top_parent) {
			printf("\n-- Practical constraints (%s): ",
				top_parent->Identifier);
			asn1print_constraint_explain(top_parent->expr_type,
				tc->combined_constraints, 0);
			printf("\n-- PER-visible constraints (%s): ",
				top_parent->Identifier);
			asn1print_constraint_explain(top_parent->expr_type,
				tc->combined_constraints, 1);
		}
		printf("\n");
	}

	if(flags & APF_PRINT_CLASS_MATRIX
	&& tc->expr_type == A1TC_CLASSDEF) do {
		int r, col, maxidlen;
		if(tc->object_class_matrix.rows == 0) {
			printf("\n-- Class matrix is empty");
			break;
		}
		printf("\n-- Class matrix has %d entr%s:\n",
				tc->object_class_matrix.rows,
				tc->object_class_matrix.rows==1 ? "y" : "ies");
		maxidlen = tc->object_class_matrix.max_identifier_length;
		for(r = -1; r < tc->object_class_matrix.rows; r++) {
			struct asn1p_ioc_row_s *row;
			row = tc->object_class_matrix.row[r<0?0:r];
			if(r < 0) printf("--    %s", r > 9 ? " " : "");
			else printf("-- [%*d]", r > 9 ? 2 : 1, r+1);
			for(col = 0; col < row->columns; col++) {
				struct asn1p_ioc_cell_s *cell;
				cell = &row->column[col];
				if(r < 0) {
					printf("[%*s]", maxidlen,
						cell->field->Identifier);
					continue;
				}
				if(!cell->value) {
					printf(" %*s ", maxidlen, "<no entry>");
					continue;
				}
				printf(" %*s ", maxidlen,
					cell->value->Identifier);
			}
			printf("\n");
		}
	} while(0);

	if(flags & APF_PRINT_CLASS_MATRIX
	&& tc->lhs_params) do {
		int i;
		if(tc->specializations.pspecs_count == 0) {
			printf("\n-- No specializations found\n");
			break;
		}
		printf("\n-- Specializations list has %d entr%s:\n",
			tc->specializations.pspecs_count,
			tc->specializations.pspecs_count == 1 ? "y" : "ies");
		for(i = 0; i < tc->specializations.pspecs_count; i++) {
			asn1p_expr_t *se;
			struct asn1p_pspec_s *pspec;
			pspec = &tc->specializations.pspec[i];
			printf("-- ");
			TQ_FOR(se, &(pspec->rhs_pspecs->members), next) {
				asn1print_expr(asn, mod, se, flags, level+1);
			}
			printf("\n");
		}
	} while(0);

	return 0;
}


static int
asn1print_expr_dtd(asn1p_t *asn, asn1p_module_t *mod, asn1p_expr_t *expr, enum asn1print_flags flags, int level) {
	asn1p_expr_t *se;
	int expr_unordered = 0;
	int dont_involve_children = 0;

	switch(expr->meta_type) {
	case AMT_TYPE:
	case AMT_TYPEREF:
		break;
	default:
		if(expr->expr_type == A1TC_UNIVERVAL)
			break;
		return 0;
	}

	if(!expr->Identifier) return 0;

	if(flags & APF_LINE_COMMENTS)
		INDENT("<!-- #line %d -->\n", expr->_lineno);
	INDENT("<!ELEMENT %s", expr->Identifier);

	if(expr->expr_type == A1TC_REFERENCE) {
		se = asn1f_find_terminal_type_ex(asn, expr);
		if(!se) {
			printf(" (ANY)");
			return 0;
		}
		expr = se;
		dont_involve_children = 1;
	}

	if(expr->expr_type == ASN_CONSTR_CHOICE
	|| expr->expr_type == ASN_CONSTR_SEQUENCE_OF
	|| expr->expr_type == ASN_CONSTR_SET_OF
	|| expr->expr_type == ASN_CONSTR_SET
	|| expr->expr_type == ASN_BASIC_INTEGER
	|| expr->expr_type == ASN_BASIC_ENUMERATED) {
		expr_unordered = 1;
	}

	if(TQ_FIRST(&expr->members)) {
		int extensible = 0;
		printf(" (");
		TQ_FOR(se, &(expr->members), next) {
			if(se->expr_type == A1TC_EXTENSIBLE) {
				extensible = 1;
				continue;
			} else if(!se->Identifier
					&& se->expr_type == A1TC_REFERENCE) {
				asn1print_ref(se->reference, flags);
			} else if(se->Identifier) {
				printf("%s", se->Identifier);
			} else {
				printf("ANY");
			}
			if(expr->expr_type != ASN_CONSTR_SET
			&& expr->expr_type != ASN_CONSTR_CHOICE
			&& expr->expr_type != ASN_BASIC_INTEGER
			&& expr->expr_type != ASN_BASIC_ENUMERATED) {
				if(expr_unordered)
					printf("*");
				else if(se->marker.flags)
					printf("?");
			}
			if(TQ_NEXT(se, next)
			&& TQ_NEXT(se, next)->expr_type != A1TC_EXTENSIBLE) {
				printf(expr_unordered?"|":", ");
			}
		}
		if(extensible) {
			printf(expr_unordered?"|":", ");
			printf("ANY");
			if(expr->expr_type != ASN_CONSTR_SET
			&& expr->expr_type != ASN_CONSTR_CHOICE
			&& expr->expr_type != ASN_BASIC_INTEGER
			&& expr->expr_type != ASN_BASIC_ENUMERATED)
				printf("*");
		}

		printf(")");
		if(expr->expr_type == ASN_CONSTR_SET)
			printf("*");

	} else switch(expr->expr_type) {
	case ASN_BASIC_BOOLEAN:
		printf(" (true|false)");
		break;
	case ASN_CONSTR_CHOICE:
	case ASN_CONSTR_SET:
	case ASN_CONSTR_SET_OF:
	case ASN_CONSTR_SEQUENCE:
	case ASN_CONSTR_SEQUENCE_OF:
	case ASN_BASIC_NULL:
	case A1TC_UNIVERVAL:
		printf(" EMPTY");
		break;
	case ASN_TYPE_ANY:
		printf(" ANY");
		break;
	case ASN_BASIC_BIT_STRING:
	case ASN_BASIC_OCTET_STRING:
	case ASN_BASIC_OBJECT_IDENTIFIER:
	case ASN_BASIC_RELATIVE_OID:
	case ASN_BASIC_INTEGER:
	case ASN_BASIC_UTCTime:
	case ASN_BASIC_GeneralizedTime:
	case ASN_STRING_NumericString:
	case ASN_STRING_PrintableString:
		printf(" (#PCDATA)");
		break;
	case ASN_STRING_VisibleString:
	case ASN_STRING_ISO646String:
		/* Entity references, but not XML elements may be present */
		printf(" (#PCDATA)");
		break;
	case ASN_BASIC_REAL:		/* e.g. <MINUS-INFINITY/> */
	case ASN_BASIC_ENUMERATED:	/* e.g. <enumIdentifier1/> */
	default:
		/*
		 * XML elements are allowed.
		 * For example, a UTF8String may contain "<bel/>".
		 */
		printf(" ANY");
	}
	printf(">\n");

	/*
	 * Display the descendants (children) of the current type.
	 */
	if(!dont_involve_children) {
		TQ_FOR(se, &(expr->members), next) {
			if(se->expr_type == A1TC_EXTENSIBLE) continue;
			asn1print_expr_dtd(asn, mod, se, flags, level + 1);
		}
	}

	return 0;
}
