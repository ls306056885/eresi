/*
 * links.c in libmjollnir for ERESI
 *
 * All the functions that deal with linking other objects, such
 * as functions, blocks, or others.
 *
 * Started on Fri Jun 22 2007 mayhem
 * $Id: links.c,v 1.1 2007-06-22 16:58:26 may Exp $
 */
#include <libmjollnir.h>



/**
 * Link function layer
 * @param ctxt mjollnir context structure
 * @param str source address
 * @param dst destination address
 * @param ret return address
 */
int			mjr_functions_link_call(mjrcontext_t *ctxt, 
						elfsh_Addr src, 
						elfsh_Addr dst, 
						elfsh_Addr ret)
{
  mjrcontainer_t	*fun;
  mjrfunc_t		*tmpfunc;
  char			*tmpstr,*md5;
  elfsh_Addr		tmpaddr;
 
  PROFILER_IN(__FILE__, __FUNCTION__, __LINE__);

#if __DEBUG_FUNCS__
  fprintf(D_DESC, "[D] %s: src:%x dst:%x ret:%x\n", __FUNCTION__, src, dst, ret);
#endif

  /* Link/Prepare function layer. We use an intermediate variable, else
   the compiler optimize too hard and that make segfault (bug in gcc ?) */
  tmpaddr = dst;
  fun = mjr_function_get_by_vaddr(ctxt, tmpaddr);
  if (!fun)
    {
      tmpstr = _vaddr2str(tmpaddr);
      fun = mjr_create_function_container(ctxt, tmpaddr, 0, tmpstr, NULL, NULL);
      mjr_function_register(ctxt,tmpaddr, fun);
    }

  /* Add links between functions */
  if (ctxt->curfunc)
    {
      mjr_container_add_link(ctxt, fun, ctxt->curfunc->id, 
			     MJR_LINK_FUNC_RET, MJR_LINK_IN);
      mjr_container_add_link(ctxt, ctxt->curfunc, fun->id, 
			     MJR_LINK_FUNC_CALL, MJR_LINK_OUT);
    }

  // when a function start, we do a fingerprint of it
  md5 = mjr_fingerprint_function(ctxt, tmpaddr, MJR_FPRINT_TYPE_MD5);

  tmpfunc = (mjrfunc_t *) fun->data;

  if (md5)
    snprintf(tmpfunc->md5,sizeof(tmpfunc->md5),"%s",md5);

  PROFILER_ROUT(__FILE__, __FUNCTION__, __LINE__, 0);
}





int		mjr_block_relink(mjrcontext_t *ctx,
				 mjrcontainer_t *src,
				 mjrcontainer_t *dst,
				 int direction)
{

  mjrlink_t	*lnk;
  u_int		nbr;

  PROFILER_IN(__FILE__, __FUNCTION__, __LINE__);

  lnk = mjr_link_get_by_direction(src, direction);

#if __DEBUG_BLOCKS__
  fprintf(D_DESC,"[D] %s: src:%d dst:%d dir:%d\n",
	  __FUNCTION__, src->id, dst->id, direction);
#endif

  if (direction == MJR_LINK_IN)
    nbr = src->in_nbr;
  else
    nbr = src->out_nbr;

  while(lnk)
    {
      // 1 - add same links to dst
      mjr_container_add_link(ctx, dst,lnk->id, lnk->type, direction);
      // 2 - and remove it from src
      lnk->type = MJR_LINK_DELETE;
      lnk = lnk->next;
    }

  mjr_container_link_cleanup(src,direction);

  PROFILER_ROUT(__FILE__, __FUNCTION__, __LINE__, 1);
}



/* Create a link between blocks on a call */
int	mjr_blocks_link_call(mjrcontext_t *ctxt,
			     elfsh_Addr src,
			     elfsh_Addr dst,
			     elfsh_Addr ret)
{
  mjrcontainer_t	*csrc,*cdst,*cret;

  PROFILER_IN(__FILE__, __FUNCTION__, __LINE__);

#if __DEBUG_BLOCKS__
  fprintf(D_DESC,"[D] %s: linking %x CALL %x RET %x\n",
	  __FUNCTION__,
	  src,dst,ret);
#endif

  /* at this point we must have src block */
  csrc = mjr_block_get_by_vaddr(ctxt, src, MJR_BLOCK_GET_FUZZY);

  assert(csrc != NULL);

  /* search and/or split destination block */
  if (!(cdst = mjr_split_block(ctxt,dst,NULL)))
    PROFILER_ERR(__FILE__,__FUNCTION__,__LINE__,
		 "Could not split the dst",0);

  /* search and/or split - return block */
  if (!(cret = mjr_split_block(ctxt,ret,NULL)))
    PROFILER_ERR(__FILE__,__FUNCTION__,__LINE__,
		 "Could not split the ret",0);
  
  /* link src and dst */
  mjr_container_add_link(ctxt, csrc, cdst->id, MJR_LINK_FUNC_CALL, MJR_LINK_OUT);
  mjr_container_add_link(ctxt, cdst, csrc->id, MJR_LINK_FUNC_CALL, MJR_LINK_IN);

  /* link dst and ret */
  mjr_container_add_link(ctxt, cdst, cret->id, MJR_LINK_FUNC_RET, MJR_LINK_OUT);
  mjr_container_add_link(ctxt, cret, cdst->id, MJR_LINK_FUNC_RET, MJR_LINK_IN);

  // mjr_block_relink_cond_always(csrc,cret,MJR_LINK_OUT);
  // mjr_block_relink_cond_always(cret,csrc,MJR_LINK_IN);

  mjr_container_add_link(ctxt, csrc, cret->id, MJR_LINK_TYPE_DELAY, MJR_LINK_OUT);
  mjr_container_add_link(ctxt, cret, csrc->id, MJR_LINK_TYPE_DELAY, MJR_LINK_IN);

#if __DEBUG_BLOCKS__
  mjr_block_dump(ctxt,csrc);
  mjr_block_dump(ctxt,cdst);
  mjr_block_dump(ctxt,cret);
#endif

  PROFILER_ROUT(__FILE__, __FUNCTION__, __LINE__, 1);
}


/** This function does prepare linking of blocks on conditional jumps
 **/
int	mjr_blocks_link_jmp(mjrcontext_t *ctxt,
			    elfsh_Addr src,
			    elfsh_Addr dst,
			    elfsh_Addr ret)
{
  mjrcontainer_t	*csrc,*cdst,*cret;
  
  PROFILER_IN(__FILE__, __FUNCTION__, __LINE__);
  
#if __DEBUG_BLOCKS__
  fprintf(D_DESC,"[D] %s: linking JMP %x TRUE %x FALSE %x\n",
	  __FUNCTION__,
	  src,dst,ret);
#endif
  
  if (!(cdst = mjr_split_block(ctxt,dst,MJR_LINK_BLOCK_COND_ALWAYS)))
    PROFILER_ERR(__FILE__,__FUNCTION__,__LINE__,
		 "Could not split the dst",0);
  
  csrc = mjr_block_get_by_vaddr(ctxt, src, MJR_BLOCK_GET_FUZZY);
  
  assert(csrc != NULL);
  
  cret = NULL;
  
  if (ret)
    if (!(cret = mjr_split_block(ctxt,ret,MJR_LINK_BLOCK_COND_ALWAYS)))
      PROFILER_ERR(__FILE__,__FUNCTION__,__LINE__,
		   "Could not split the ret",0);
  
  mjr_container_add_link(ctxt, csrc, cdst->id, MJR_LINK_BLOCK_COND_TRUE, MJR_LINK_OUT);
  mjr_container_add_link(ctxt, cdst, csrc->id, MJR_LINK_BLOCK_COND_TRUE, MJR_LINK_IN);  
  if (cret)
    {
      mjr_container_add_link(ctxt, csrc, cret->id, MJR_LINK_BLOCK_COND_FALSE, MJR_LINK_OUT);
      mjr_container_add_link(ctxt, cret, csrc->id, MJR_LINK_BLOCK_COND_FALSE, MJR_LINK_IN);
    }

  PROFILER_ROUT(__FILE__, __FUNCTION__, __LINE__, 1);
}



/**
 * This function does split a block carried by given container
 * to 2 pices, a new container will be added by vaddr dst
 * @param ctxt mjollnir context strucutre
 * @param dst  destination address of wanted block
 * @param link_with link splitted blocks with specified link type
 */
mjrcontainer_t	*mjr_split_block(mjrcontext_t *ctxt,
				 elfsh_Addr dst,
				 u_int link_with)
{
  mjrcontainer_t	*tmpdst,*dstend;
  mjrblock_t		*blkdst;
  int			new_size;

  PROFILER_IN(__FILE__, __FUNCTION__, __LINE__);

  tmpdst = mjr_block_get_by_vaddr(ctxt, dst, MJR_BLOCK_GET_FUZZY);

  if (!tmpdst)
    {
      tmpdst = mjr_create_block_container(ctxt, 0, dst, 1);
      hash_add(&ctxt->blkhash, _vaddr2str(dst), tmpdst);
      PROFILER_ROUT(__FILE__, __FUNCTION__, __LINE__, (tmpdst));
    }

  blkdst = (mjrblock_t *)tmpdst->data;

#if __DEBUG_BLOCKS__
  fprintf(D_DESC,"[D] %s: wanted dst:%x got:%x\n",
	  __FUNCTION__,dst,blkdst->vaddr);
#endif

  if (blkdst->vaddr != dst)
    {
      new_size		= blkdst->size - (dst - blkdst->vaddr);
      blkdst->size	-= new_size;

      assert(new_size > 0);
      assert(blkdst->size > 0);

      dstend		= mjr_create_block_container(ctxt, 0, dst, new_size);
      hash_add(&ctxt->blkhash, _vaddr2str(dst), dstend);

      if (link_with != NULL)
	{
	  mjr_block_relink(ctxt, tmpdst, dstend, MJR_LINK_OUT);
	  mjr_container_add_link(ctxt, tmpdst, dstend->id, link_with, MJR_LINK_OUT);
	  mjr_container_add_link(ctxt, dstend, tmpdst->id, link_with, MJR_LINK_IN);
	}
    } 
  else 
    {
      dstend = tmpdst;
    }

  PROFILER_ROUT(__FILE__, __FUNCTION__, __LINE__, (dstend));
}