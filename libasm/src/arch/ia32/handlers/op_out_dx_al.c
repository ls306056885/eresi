/*
** $Id$
**
*/
#include <libasm.h>
#include <libasm-int.h>

/*
  <instruction func="op_out_dx_al" opcode="0xee"/>
 */

int     op_out_dx_al(asm_instr *new, u_char *opcode, u_int len,
                         asm_processor *proc)
{
  new->len += 1;
  new->ptr_instr = opcode;
  new->instr = ASM_OUT;
  new->type = ASM_TYPE_IO | ASM_TYPE_STORE;

  new->len += asm_operand_fetch(&new->op[0], opcode, ASM_CONTENT_FIXED, new);
	new->op[0].type = ASM_OPTYPE_MEM;
  new->op[0].memtype = ASM_OP_BASE | ASM_OP_REFERENCE;
  new->op[0].regset = ASM_REGSET_R16;
  new->op[0].baser = ASM_REG_DX;

  new->len += asm_operand_fetch(&new->op[1], opcode, ASM_CONTENT_FIXED, new);
  new->op[1].type = ASM_OPTYPE_REG;
  new->op[1].regset = ASM_REGSET_R8;
  new->op[1].baser = ASM_REG_AL;

  return (new->len);
}
