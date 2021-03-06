/* IBM_PROLOG_BEGIN_TAG                                                   */
/* This is an automatically generated prolog.                             */
/*                                                                        */
/* $Source: src/block/test/blk_api_tst.c $                                */
/*                                                                        */
/* IBM Data Engine for NoSQL - Power Systems Edition User Library Project */
/*                                                                        */
/* Contributors Listed Below - COPYRIGHT 2014,2015                        */
/* [+] International Business Machines Corp.                              */
/*                                                                        */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/*                                                                        */
/* IBM_PROLOG_END_TAG                                                     */
#include "blk_tst.h"
#include <pthread.h>
#include <fcntl.h>

#ifndef _AIX
#include <libudev.h>
#endif


char test_filename[256];
pthread_t blk_thread[MAX_NUM_THREADS];


pthread_mutex_t  completion_lock;

extern int num_opens;
extern uint32_t thread_count;
extern uint64_t block_number;
extern int num_loops;
extern int thread_flag;
extern int  num_threads;
extern uint64_t  max_xfer;;
extern int virt_lun_flags;
extern int share_cntxt_flags;
extern int  test_max_cntx;
extern char* env_filemode;
extern char* env_max_xfer;
extern char* env_num_cntx;
extern char* env_num_list;
extern int io_bufcnt;
extern int num_listio;

char def_capidev[50] = "/dev/cxl/afu0.0s";

char        *dev_paths[MAX_LUNS];

char *env_blk_verbosity = NULL;

char *env_num_list = NULL;


char *parents [MAX_LUNS] ;

int  blk_verbosity = 0;
int  num_devs = 0;
int  filemode = 0;

blk_thread_data_t blk_data;

char *paths;
char tpaths[512];

void initialize_blk_tests()
{
    int rc =0;
    rc = cblk_init(NULL,0);
    if (rc){
        fprintf(stderr, "cblk_term: failed . errno =%d\n", errno);
        return;
    }
    io_bufcnt = 4;

}

void teminate_blk_tests()
{
    int rc =0;
    rc = cblk_term(NULL,0);
    if (rc){
        fprintf(stderr, "cblk_term: failed . errno =%d\n", errno);
        return;
    }
}

int blk_fvt_setup(int size)

{

    char *env_user = getenv("USER");
    int  fd;

    char *p;

    int i,ret;

    paths = getenv("FVT_DEV");

    env_num_cntx  = getenv("MAX_CNTX");
    if (env_num_cntx && (atoi(env_num_cntx) > 0))
        test_max_cntx = atoi(env_num_cntx);

    env_blk_verbosity  = getenv("FVT_BLK_VERBOSITY");

    env_num_list = getenv ("FVT_NUM_LISTIO");

    /* user specifying num list io */
    if (env_num_list) {
        num_listio = atoi(env_num_list);	
        if ((num_listio >= 500) ||
                (!num_listio) ) {
            /* Use default if 0 or greater than 500 */
            num_listio = 500;
        }
    }

    if (env_blk_verbosity)
        blk_verbosity = atoi(env_blk_verbosity);

    DEBUG_1 ("blk_verbosity = %s \n",env_blk_verbosity);
    DEBUG_1 ("num_listio = %d \n",num_listio);

    DEBUG_1("env_filemode = %s\n",env_filemode);
    DEBUG_1("env_max_xfer = %s\n",env_max_xfer);

    if (env_max_xfer) {
        max_xfer = atoi(env_max_xfer);
        DEBUG_1("max_xfer_size = 0x%lx\n",max_xfer);
    }
    if (paths == NULL) {
        if ((env_filemode) &&  (filemode=atoi(env_filemode) == 1)) {
            sprintf(test_filename, "/tmp/%s.capitestfile", env_user);

            fd = creat(test_filename, 0777);
            if (fd != -1) { 
                paths = &test_filename[0];
                ret = ftruncate (fd, FILESZ);
                if (ret){
                    fprintf(stderr,"blk_fvt_setup: couldn't increase  filesize\n");
                    fprintf (stderr,"\nftruncate: rc = %d, err = %d\n", ret, errno);
                    close (fd);
                }
            } else {
                fprintf(stderr,"blk_fvt_setup: couldn't create test file\n");
                fprintf(stderr,"blk_fvt_setup: fd = %d, errno = %d\n", fd, errno);
                return (-1);
            }
        } else {
            fprintf(stderr,"Environment FVT_DEV is not set\n");
            return (-1);
        }
    }

    /* strcpy instead */
    strcpy (&tpaths[0], paths);


    DEBUG_2("\nsaving paths = %s to tpaths = %s\n",paths,tpaths);

    p = strtok(&tpaths[0], "," );
    for (i=0; p != NULL; i++ ) {
        if (!filemode) {
            parents[i] = find_parent(p);
            DEBUG_2("\nparent of %s p = %s\n", p, parents[i]);
        }
        dev_paths[i] = p;
        DEBUG_2("\npath %d : %s\n", i, p);
        p = strtok((char *)NULL,",");

    } 
    num_devs = i;
    DEBUG_1("blk_fvt_setup: num_devs = %d\n",num_devs);

    bzero (&blk_data, sizeof(blk_data));
    DEBUG_1("blk_fvt_setup: path = %s\n", dev_paths[0]);
    DEBUG_1("blk_fvt_setup: allocating blocks %d\n", size);
    ret = blk_fvt_alloc_bufs(size);
    DEBUG_1("blk_fvt_alloc_bufs: ret = 0x%x\n", ret);

    DEBUG_2("blk_fvt_setupi_exit: dev1 = %s, dev2 = %s\n", 
            dev_paths[0], dev_paths[1]);
    return (ret);

}
/* check parents of all devs, and see if the context is sharable */
int validate_share_context()
{
    int n,i=0;
    int rc = 0;
    char *p;
    n = num_devs;
    p = parents[i++];
    DEBUG_1("\nvalidate_share_context: num_devs = %d ", n);
    for (; i < n ; i++) {
        DEBUG_2("\nvalidate_share_context: %s\n and %s\n ", p, parents[i]);
        rc = strcmp (p, parents[i]);
        if (rc != 0)
            break;
    }

    DEBUG_1("\nvalidate_share_context: ret = 0x%x\n", rc);
    return(rc);

}

void blk_open_tst_inv_path (const char* path, int *id, int max_reqs, int *er_no, int opn_cnt, int flags, int mode)
{
    chunk_id_t j = NULL_CHUNK_ID;
    chunk_ext_arg_t ext = 0;
    errno = 0;

    j = cblk_open (path, max_reqs, mode, ext, flags);

    *id = j;
    *er_no = errno; 
}

void blk_open_tst (int *id, int max_reqs, int *er_no, int opn_cnt, int flags, int mode)
{

    // chunk_id_t handle[MAX_OPENS+15];

    int             i = 0;
    chunk_id_t j = NULL_CHUNK_ID;
    chunk_ext_arg_t ext = 0;
    errno = 0;

    DEBUG_2("blk_open_tst : open %s %d times\n",dev_paths[0],opn_cnt);

    if (opn_cnt > 0) {
        for (i = 0; i!= opn_cnt; i++) {
            DEBUG_1("Opening %s\n",dev_paths[0]);
            j = cblk_open (dev_paths[0], max_reqs, mode, ext, flags);
            if (j != NULL_CHUNK_ID) {
                // handle[i] = j;
                chunks[i] = j;
                num_opens += 1;
                DEBUG_2("blk_open_tst:  OPENED %d, chunk_id=%d\n", (num_opens), j);
            } else {
                *id = j;
                *er_no = errno;
                DEBUG_2("blk_open_tst: Failed: open i = %d, errno = %d\n", 
                        i, errno);
                return;
            }
        }
        *id = j; 
    }
    DEBUG_2("blk_open_tst: id = %d, num_open = %d\n", j, num_opens);
}

void blk_close_tst(int id, int *rc, int *err, int close_flag)
{

    errno = 0;

    *rc = cblk_close (id, close_flag);
    *err = errno;
    // it should be done in cleanup
    if ( !(*rc)  && !(*err))
        num_opens --;
    DEBUG_3("blk_close_tst: id = %d, erno = %d, rc = %d\n", id, errno, *rc);
}

void blk_open_tst_cleanup ()
{

    int             i  = 0;
    int             rc = 0;

    errno = 0;

    DEBUG_1("blk_open_tst_cleanup: closing num_opens = %d\n", num_opens);
    for (i=0; num_opens |= 0; i++) {
        if (chunks[i] != NULL_CHUNK_ID) {
            rc =  cblk_close (chunks[i],0);
            if (!rc) {
                DEBUG_1("blk_open_tst_cleanup: closed %d\n", num_opens);
                chunks[i] = NULL_CHUNK_ID;
                num_opens--;  
            } else {
                DEBUG_3("\nblk_open_tst_cleanup: Close FAILED chunk id = %d , rc = %d, errno %d\n",
                        chunks[i], rc, errno);
                break;
            }
        }
    }

    DEBUG_1("blk_open_tst_cleanup: ***Num Chunk CLOSED %d\n\n",i);

    // At the end of test executions, to free up allocated
    // test buffers.

    if (blk_fvt_data_buf != NULL)
        free(blk_fvt_data_buf);
    if (blk_fvt_comp_data_buf != NULL)
        free(blk_fvt_comp_data_buf);


}


void blk_fvt_get_set_lun_size(chunk_id_t id, 
                              size_t *size, 
                              int sz_flags, 
                              int get_set_size_flag,
                              int *ret,
                              int *err)
{
    size_t c_size;
    int rc;
    errno =0;

    if (!get_set_size_flag) {
        /* get physical lun size */
        rc = cblk_get_lun_size(id, &c_size, sz_flags);
        *err = errno;
        *size = c_size;
        *ret = rc;
        DEBUG_3("get_lun_size: sz = %d, ret = %d, err = %d\n",
                (int)c_size, rc, errno);
    } else if (get_set_size_flag == 1) {
        /* get chunk size */
        rc= cblk_get_size(id, &c_size, sz_flags);
        *err = errno;
        *ret = rc;
        *size = c_size;
        DEBUG_3("get_size: sz = %d, ret = %d, err = %d\n",
                (int)c_size, rc, errno);
    } else if (get_set_size_flag == 2) {
        /* set chunk size */
        c_size = *size;
        rc= cblk_set_size(id, c_size, sz_flags);
        *err = errno;
        *ret = rc;
        DEBUG_3("set_size: sz = %d, ret = %d, err = %d\n",
                (int)c_size, rc, errno);
    }

}

int blk_fvt_alloc_large_xfer_io_bufs(size_t size)
{

    int ret = -1;


    if (blk_fvt_data_buf != NULL)
        free(blk_fvt_data_buf);
    if (blk_fvt_comp_data_buf != NULL)
        free(blk_fvt_comp_data_buf);




    /*
     * Align data buffer on page boundary.
     */	
    if ( posix_memalign((void *)&blk_fvt_data_buf,4096,BLK_FVT_BUFSIZE*size)) {

        perror("posix_memalign failed for data buffer");
        return (ret);
    }
    bzero(blk_fvt_data_buf,BLK_FVT_BUFSIZE*size);

    /*
     * Align data buffer on page boundary.
     */	
    if ( posix_memalign((void *)&blk_fvt_comp_data_buf,4096,BLK_FVT_BUFSIZE*size)) {
        perror("posix_memalign failed for comp data buffer");
        free(blk_fvt_data_buf);
        return (ret);

    }

    return(0);
}



int blk_fvt_alloc_bufs(int size)
{

    int ret = -1;
    int i,x;
    char* p;

    /*
     * Align data buffer on page boundary.
     */	
    if ( posix_memalign((void *)&blk_fvt_data_buf,4096,BLK_FVT_BUFSIZE*size)) {

        perror("posix_memalign failed for data buffer");
        return (ret);
    }

    bzero(blk_fvt_data_buf,BLK_FVT_BUFSIZE*size);

    /*
     * Align data buffer on page boundary.
     */	
    if ( posix_memalign((void *)&blk_fvt_comp_data_buf,4096,BLK_FVT_BUFSIZE*size)) {
        perror("posix_memalign failed for comp data buffer");
        free(blk_fvt_data_buf);
        return (ret);

    }
    bzero(blk_fvt_comp_data_buf,BLK_FVT_BUFSIZE*size);

    p = blk_fvt_comp_data_buf;
    for (i=1; i<size+1;i++) {
        /* fill each page with diff data to write */
        for (x=0; x<BLK_FVT_BUFSIZE; x++) {
            *p=i;
            p++;
        }     
    }
    return(0);
}


int blk_fvt_alloc_list_io_bufs(int size)
{

    int ret = -1;
    int fd;


    if (blk_fvt_data_buf != NULL)
        free(blk_fvt_data_buf);
    if (blk_fvt_comp_data_buf != NULL)
        free(blk_fvt_comp_data_buf);


    /*
     * Align data buffer on page boundary.
     */	
    if ( posix_memalign((void *)&blk_fvt_data_buf,4096,BLK_FVT_BUFSIZE*size)) {

        perror("posix_memalign failed for data buffer");
        return (ret);
    }
    bzero(blk_fvt_data_buf,BLK_FVT_BUFSIZE*size);

    /*
     * Align data buffer on page boundary.
     */	
    if ( posix_memalign((void *)&blk_fvt_comp_data_buf,4096,BLK_FVT_BUFSIZE*size)) {
        perror("posix_memalign failed for comp data buffer");
        free(blk_fvt_data_buf);
        return (ret);

    }

    bzero(blk_fvt_comp_data_buf,BLK_FVT_BUFSIZE*size);
    /* fill compare buffer with pattern */
    fd = open ("/dev/urandom", O_RDONLY);
    read (fd, blk_fvt_comp_data_buf, BLK_FVT_BUFSIZE*size);
    close (fd);
    return(0);
}

void blk_fvt_cmp_buf (int size, int *ret)
{
    int rc;

    rc = memcmp(blk_fvt_data_buf,blk_fvt_comp_data_buf, BLK_FVT_BUFSIZE*size);
    if (rc) {
#ifdef _AIX
        DEBUG_1("Memcmp failed with rc = 0x%x\n",rc);
#else
        DEBUG_1("Memcmp failed with rc = 0x%x\n",rc);
#endif
        /**
          DEBUG_0("Written data:\n");
          dumppage(blk_fvt_comp_data_buf,BLK_FVT_BUFSIZE);
          DEBUG_0("*********************************************************\n\n");
          DEBUG_0("read data:\n");
          dumppage(blk_fvt_data_buf,BLK_FVT_BUFSIZE);
         **/
    } else {
        DEBUG_1("Memcmp OK with rc = 0x%x\n",rc);
    }
    *ret = rc;
}

void blk_fvt_io (chunk_id_t id, int cmd, uint64_t lba_no, size_t nblocks, int *ret, int *err, int io_flags, int open_flag)
{
    int	rc = 0;
    errno = 0;
    int tag = 0;
    cblk_arw_status_t ard_status;
    cblk_arw_status_t awrt_status;
    uint64_t ar_status = 0;

    int align = 0;
    int arflag = 0;

    align = (io_flags & FV_ALIGN_BUFS) ? 1: 0;
    arflag = (io_flags & FV_ARESULT_BLOCKING) ? CBLK_ARESULT_BLOCKING : 0;
    arflag |= (io_flags & FV_ARESULT_NEXT_TAG) ? FV_ARESULT_NEXT_TAG : 0;

    switch  (cmd) {
        case FV_READ:
            rc = cblk_read(id,(blk_fvt_data_buf+align),lba_no,nblocks,0);
            DEBUG_3("cblk_read complete for lba = 0x%lx, rc = %d, errno = %d\n",lba_no,rc,errno);
            if (rc <= 0) {
                if(blk_verbosity == 9) {
                    fprintf(stderr,"cblk_read failed for  lba = 0x%lx, rc = %d, errno = %d\n",lba_no,rc,errno);
                    fprintf(stderr,"Returned data is ...\n");
                    hexdump(blk_fvt_data_buf,100,NULL);
                }
            }
            break;
        case FV_AREAD:

            if ((open_flag & CBLK_OPN_NO_INTRP_THREADS) &&
                    (io_flags & CBLK_ARW_USER_STATUS_FLAG)) {
                rc = cblk_aread(id, blk_fvt_data_buf+align, lba_no, nblocks, &tag, &ard_status, io_flags);
            } else {
                rc = cblk_aread(id, blk_fvt_data_buf+align, lba_no, nblocks,
                        &tag, NULL, io_flags);
            }

            if (rc < 0) {
                DEBUG_3("cblk_aread error  lba = 0x%lx, rc = %d, errno = %d\n", lba_no, rc, errno);
                *ret = rc;
                *err = errno;
                return;
            } else {
                DEBUG_2("Async read returned rc = %d, tag =%d\n", rc, tag);
                while (TRUE) {

                    rc = cblk_aresult(id,&tag, &ar_status,arflag);
                    if (rc > 0) {
                        if (blk_verbosity) {
                            DEBUG_0("Async read data completed ...\n");
                            if (blk_verbosity == 9) {
                                DEBUG_0("Returned data is ...\n");
                                hexdump(blk_fvt_comp_data_buf,100,NULL);
                            }
                        }
                    } else if (rc == 0) {
                        DEBUG_3("cblk_aresult completed: command still active  for tag = 0x%x, rc = %d, errno = %d\n",tag,rc,errno);
                        usleep(1000);
                        continue;
                    } else {
                        DEBUG_3("cblk_aresult completed for  for tag = 0x%d, rc = %d, errno = %d\n",tag,rc,errno);
                    }

                    break;

                } /* while */
            }
            break;

        case FV_WRITE:
            rc = cblk_write(id,blk_fvt_comp_data_buf+align,lba_no,nblocks,0);
            DEBUG_4("cblk_write complete at lba = 0x%lx, size = %lx, rc = %d, errno = %d\n",lba_no,nblocks,rc,errno);
            if (rc <= 0) {
                DEBUG_3("cblk_write failed at lba = 0x%lx, rc = %d, errno = %d\n",lba_no,rc,errno);
            }
            break;
        case FV_AWRITE:
            if ((open_flag & FV_NO_INRPT) &&
                    (io_flags & CBLK_ARW_USER_STATUS_FLAG)) {
                rc = cblk_awrite(id, blk_fvt_comp_data_buf+align, lba_no, nblocks, &tag, &awrt_status, io_flags);
            } else {
                rc = cblk_awrite(id, blk_fvt_comp_data_buf+align, lba_no, nblocks,
                        &tag, NULL, io_flags);
            }

            if (rc < 0) {
                DEBUG_3("cblk_awrite error  lba = 0x%lx, rc = %d, errno = %d\n", lba_no, rc, errno);
                *ret = rc;
                *err = errno;
                return;
            } else {
                while (TRUE) {

                    rc = cblk_aresult(id,&tag, &ar_status,arflag);
                    if (rc > 0) {
                        if (blk_verbosity) {
                            DEBUG_0("Async write data completed ...\n");
                            if (blk_verbosity == 9) {
                                DEBUG_0("Returned data is ...\n");
                                hexdump(blk_fvt_comp_data_buf,100,NULL);
                            }
                        }
                    } else if (rc == 0) {
                        DEBUG_3("cblk_aresult completed: command still active  for tag = 0x%x, rc = %d, errno = %d\n",tag,rc,errno);
                        usleep(1000);
                        continue;
                    } else {
                        DEBUG_3("cblk_aresult completed for  for tag = 0x%d, rc = %d, errno = %d\n",tag,rc,errno);
                    }

                    break;

                } /* while */
            }
            break;

        default:
            break;
    }
    *ret = rc;
    *err = errno;

    DEBUG_2("blk_fvt_io: ret = %d, errno = %d\n", rc, errno);

}

void
blk_fvt_intrp_io_tst(chunk_id_t id,
           int testflag,
	   int open_flag,
	   int *ret,
	   int *err)
{
    int             rc = 0;
    int             err_no = 0;
    errno = 0;
    int             tag = 0;
    uint64_t	lba_no = 1;
    int		nblocks = 1;
    cblk_arw_status_t arw_status;
    int             fd;
    int             arflag = 0;
    int             io_flags = 0;

    bzero(&arw_status,sizeof(arw_status));

    /* fill compare buffer with pattern */
    fd = open("/dev/urandom", O_RDONLY);
    read(fd, blk_fvt_comp_data_buf, BLK_FVT_BUFSIZE);
    close(fd);

    // testflag 1 - NO_INTRP      set, null status,  io_flags ARW_USER  set
    // testflag 2 - NO_INTRP _not set,      status,  io_flags ARW_USER  not set
    // testflag 3 - NO_INTRP _not set,      status,  io_flags ARW_USER  set
    // testflag 4 - NO_INTRP      set,      status,  io_flags ARW_USER  set
    // testflag 5 - NO_INTRP      set,      status,  ARW_USER | ARW_WAIT set
    // testflag 6 - NO_INTRP      set,      status,  ARW_USER | ARW_WAIT| ARW_USER_TAG set

    switch(testflag) {
        case 1:
            io_flags |= CBLK_ARW_USER_STATUS_FLAG;
            break;
        case 2:
            io_flags |= 0;
            break;
        case 3:
            io_flags |= CBLK_ARW_USER_STATUS_FLAG;
            break;
        case 4:
            io_flags |= CBLK_ARW_USER_STATUS_FLAG;
            break;
        case 5:
            io_flags |= CBLK_ARW_USER_STATUS_FLAG|CBLK_ARW_WAIT_CMD_FLAGS;
            break;
        case 6:
            io_flags |= CBLK_ARW_USER_STATUS_FLAG|CBLK_ARW_WAIT_CMD_FLAGS|CBLK_ARW_USER_TAG_FLAG;
            break;

        default:
            break;

    }

    rc = cblk_awrite(id, blk_fvt_comp_data_buf, lba_no, nblocks,
            &tag,((testflag == 1) ? NULL : (&arw_status)), io_flags);

    if (rc < 0) {
        DEBUG_3("cblk_awrite error  lba = 0x%lx, rc = %d, errno = %d\n", lba_no, rc, errno);
        *ret = rc;
        *err = errno;
        return;
    }

    check_astatus(id, &tag, arflag, open_flag, io_flags, &arw_status, &rc, &err_no);

    DEBUG_2("blk_fvt_io: ret = %d, errno = %d\n", rc, err_no);
    *ret = rc;
    *err = errno;
    return;

}

void
check_astatus(chunk_id_t id, int *tag, int arflag, int open_flag, int io_flags, cblk_arw_status_t *arw_status, int *rc, int *err)
{

    uint64_t ar_status = 0;
    int ret = 0;


    while (TRUE) {
        if ((open_flag & ~FV_NO_INRPT) &&
                (io_flags & CBLK_ARW_USER_STATUS_FLAG)) {
            switch (arw_status->status) {
                case CBLK_ARW_STATUS_SUCCESS:
                    ret = arw_status->blocks_transferred;
                    break;
                case CBLK_ARW_STATUS_PENDING:
                    ret = 0;
                    break;
                default:
                    ret = -1;
                    errno = arw_status->fail_errno;
            }

        } else {
            ret = cblk_aresult(id, tag, &ar_status, arflag);
        }
        if (ret > 0) {

            DEBUG_0("Success\n");
        } else if (ret == 0) {
            DEBUG_0("Cmd pending !\n");
            usleep(300);
            continue;
        } else {
            DEBUG_2("Cmd completed ret = %d, errno = %d\n",ret,errno);
        }

        break;

    } /* while */

    *rc = ret;
    *err = errno;

}


void blk_fvt_dump_stats(chunk_stats_t *stats)
{
    fprintf(stderr,"chunk_statistics:\n");
    fprintf(stderr,"**********************************\n");
    fprintf(stderr,"max_transfer_size:              0x%lx\n", stats->max_transfer_size);
    fprintf(stderr,"num_reads:                      0x%lx\n", stats->num_reads);
    fprintf(stderr,"num_writes:                     0x%lx\n", stats->num_writes);
    fprintf(stderr,"num_areads:                     0x%lx\n", stats->num_areads);
    fprintf(stderr,"num_awrites:                    0x%lx\n", stats->num_awrites);
    fprintf(stderr,"num_act_reads:                  0x%x\n", stats->num_act_reads);
    fprintf(stderr,"num_act_writes:                 0x%x\n", stats->num_act_writes);
    fprintf(stderr,"num_act_areads:                 0x%x\n", stats->num_act_areads);
    fprintf(stderr,"num_act_awrites:                0x%x\n", stats->num_act_awrites);
    fprintf(stderr,"max_num_act_writes:             0x%x\n", stats->max_num_act_writes);
    fprintf(stderr,"max_num_act_reads:              0x%x\n", stats->max_num_act_reads);
    fprintf(stderr,"max_num_act_awrites:            0x%x\n", stats->max_num_act_awrites);
    fprintf(stderr,"max_num_act_areads:             0x%x\n", stats->max_num_act_areads);
    fprintf(stderr,"num_blocks_read:                0x%lx\n", stats->num_blocks_read);
    fprintf(stderr,"num_blocks_written:             0x%lx\n", stats->num_blocks_written);
    fprintf(stderr,"num_errors:                     0x%lx\n", stats->num_errors);
    fprintf(stderr,"num_aresult_no_cmplt:           0x%lx\n", stats->num_aresult_no_cmplt);
    fprintf(stderr,"num_retries:                    0x%lx\n", stats->num_retries);
    fprintf(stderr,"num_timeouts:                   0x%lx\n", stats->num_timeouts);
    fprintf(stderr,"num_no_cmds_free:               0x%lx\n", stats->num_no_cmds_free);
    fprintf(stderr,"num_no_cmd_room:                0x%lx\n", stats->num_no_cmd_room);
    fprintf(stderr,"num_no_cmds_free_fail:          0x%lx\n", stats->num_no_cmds_free_fail);
    fprintf(stderr,"num_fc_errors:                  0x%lx\n", stats->num_fc_errors);
    fprintf(stderr,"num_port0_linkdowns:            0x%lx\n", stats->num_port0_linkdowns);
    fprintf(stderr,"num_port1_linkdowns:            0x%lx\n", stats->num_port1_linkdowns);
    fprintf(stderr,"num_port0_no_logins:            0x%lx\n", stats->num_port0_no_logins);
    fprintf(stderr,"num_port1_no_logins:            0x%lx\n", stats->num_port1_no_logins);
    fprintf(stderr,"num_port0_fc_errors:            0x%lx\n", stats->num_port0_fc_errors);
    fprintf(stderr,"num_port1_fc_errors:            0x%lx\n", stats->num_port1_fc_errors);
    fprintf(stderr,"num_cc_errors:                  0x%lx\n", stats->num_cc_errors);
    fprintf(stderr,"num_afu_errors:                 0x%lx\n", stats->num_afu_errors);
    fprintf(stderr,"num_capi_false_reads:           0x%lx\n", stats->num_capi_false_reads);
    fprintf(stderr,"num_capi_adap_resets:            0x%lx\n", stats->num_capi_adap_resets);
    fprintf(stderr,"num_capi_afu_errors:            0x%lx\n", stats->num_capi_afu_errors);
    fprintf(stderr,"num_capi_afu_intrpts:           0x%lx\n", stats->num_capi_afu_intrpts);
    fprintf(stderr,"num_capi_unexp_afu_intrpts:     0x%lx\n", stats->num_capi_unexp_afu_intrpts);
    fprintf(stderr,"num_active_threads:             0x%lx\n", stats->num_active_threads);
    fprintf(stderr,"max_num_act_threads:            0x%lx\n", stats->max_num_act_threads);
    fprintf(stderr,"num_cache_hits:                 0x%lx\n", stats->num_cache_hits);
    fprintf(stderr,"**********************************\n");


}

void blk_get_statistics (chunk_id_t id, int flags, int *ret, int *err)
{

    int rc;
    chunk_stats_t stats;

    rc = cblk_get_stats (id, &stats, flags);
    *ret = rc;
    *err = errno;

    if (rc) {
        DEBUG_2("blk_get_statistics: failed ret = %d, errno = %d\n", rc, errno);
    } else {
        if (blk_verbosity == 9) {
            fprintf(stderr,"cblk_get_stats completed ...\n");
            blk_fvt_dump_stats (&stats);
            hexdump(&stats,sizeof(stats),NULL);
        }
    }
}



void *blk_io_loop(void *data)
{
    int rc = 0;
    int tag;
    int i;
    blk_thread_data_t *blk_data = data;
    uint32_t local_thread_count = 0;
    void *data_buf = NULL;
    void *comp_data_buf = NULL;
    uint64_t blk_number;
    uint64_t ar_status;
    int cmd_type;
    int fd;
    int arflag = 0;
    int x, num_luns;



    pthread_mutex_lock(&completion_lock);
    local_thread_count = thread_count++;

    /*
     * Each thread is using a different
     * block number range.
     */


    blk_number = block_number + (num_loops * thread_count);

    num_luns = num_devs ;

    pthread_mutex_unlock(&completion_lock);


    /*
     * Align data buffer on page boundary.
     */	
    if ( posix_memalign((void *)&data_buf,4096,BLK_FVT_BUFSIZE)) {

        perror("posix_memalign failed for data buffer");

        blk_data->status.ret = -1;
        blk_data->status.errcode = errno;
        pthread_exit(&(blk_data->status));
    }


    errno = 0;
    if (local_thread_count % 2) {
        cmd_type = FV_RW_AWAR;
    } else {
        cmd_type = FV_RW_COMP;
    }
    for (x =0; x<num_luns; x++)  {
        for (i =0; i<num_loops; i++) {
            switch (cmd_type) {
                case FV_RW_COMP:

                    /* 
                     * Perform write then read comparision test
                     */
                    /*
                     * Align data buffer on page boundary.
                     */	
                    if ( posix_memalign((void *)&comp_data_buf,4096,BLK_FVT_BUFSIZE)) {

                        perror("posix_memalign failed for data buffer");
                        blk_data->status.ret = -1;
                        blk_data->status.errcode = errno;
                        pthread_exit(&(blk_data->status));

                    }

                    fd = open ("/dev/urandom", O_RDONLY);
                    read (fd, comp_data_buf, BLK_FVT_BUFSIZE);
                    close (fd);

                    rc = cblk_write(blk_data->chunk_id[x],comp_data_buf,blk_number,1,0);

                    if (rc != 1) {
                        blk_data->status.ret = rc;
                        blk_data->status.errcode = errno;
                        free(comp_data_buf);
                        free(data_buf);
                        pthread_mutex_lock(&completion_lock);
                        DEBUG_2("Write failed rc = %d, errno = %d\n",rc, errno);
                        pthread_mutex_unlock(&completion_lock);
                        pthread_exit(&(blk_data->status));
                    }
                    rc = cblk_read(blk_data->chunk_id[x],data_buf,blk_number,1,0);

                    if (rc != 1) {

                        blk_data->status.ret = rc;
                        blk_data->status.errcode = errno;
                        free(comp_data_buf);
                        free(data_buf);
                        pthread_mutex_lock(&completion_lock);
                        DEBUG_2("Read failed rc = %d, errno = %d\n",rc, errno);
                        pthread_mutex_unlock(&completion_lock);
                        pthread_exit(&(blk_data->status));
                    }

                    rc = memcmp(data_buf,comp_data_buf,BLK_FVT_BUFSIZE);

                    if (rc) {
                        blk_data->status.ret = rc;
                        blk_data->status.errcode = errno;
                        if(blk_verbosity == 9) {
                            pthread_mutex_lock(&completion_lock);
                            fprintf(stderr,"Data compare failure\n");
                            /*
                               fprintf(stderr,"Written data:\n");
                               dumppage(data_buf,BLK_FVT_BUFSIZE);
                               fprintf(stderr,"**********************************************************\n\n");
                               fprintf(stderr,"read data:\n");
                               dumppage(comp_data_buf,BLK_FVT_BUFSIZE);
                               fprintf(stderr,"**********************************************************\n\n");
                             */
                            pthread_mutex_unlock(&completion_lock);
                        }

                        rc = cblk_read(blk_data->chunk_id[x],data_buf,blk_number,1,0);
                        if (rc == 1) {
                            if(blk_verbosity == 9) {
                                pthread_mutex_lock(&completion_lock);
                                fprintf(stderr,"Dump of re-read\n");

                                dumppage(data_buf,BLK_FVT_BUFSIZE);
                                pthread_mutex_unlock(&completion_lock);
                            }
                        }

                    }
                    free(comp_data_buf);
                    break;
                case FV_RW_AWAR:

                    /* 
                     * Perform write then read comparision test
                     */

                    /*
                     * Align data buffer on page boundary.
                     */	
                    if ( posix_memalign((void *)&comp_data_buf,4096,BLK_FVT_BUFSIZE)) {
                        perror("posix_memalign failed for data buffer");
                        perror("posix_memalign failed for data buffer");
                        blk_data->status.ret = 0;
                        blk_data->status.errcode = errno;
                        pthread_exit(&(blk_data->status));
                    }

                    fd = open ("/dev/urandom", O_RDONLY);
                    read (fd, comp_data_buf, BLK_FVT_BUFSIZE);
                    close (fd);

                    rc = cblk_awrite(blk_data->chunk_id[x],comp_data_buf,blk_number,1,&tag,NULL,0);

                    if (rc < 0) {
                        blk_data->status.ret = rc;
                        blk_data->status.errcode = errno;
                        free(comp_data_buf);
                        free(data_buf);
                        pthread_mutex_lock(&completion_lock);
                        DEBUG_2("Awrite failed rc = %d, errno = %d\n",rc, errno);
                        pthread_mutex_unlock(&completion_lock);
                        pthread_exit(&(blk_data->status));
                    } 
                    arflag = CBLK_ARESULT_BLOCKING;
                    while (TRUE) {
                        rc = cblk_aresult(blk_data->chunk_id[x],&tag, &ar_status,arflag);
                        if (rc > 0) {
                            pthread_mutex_lock(&completion_lock);
                            DEBUG_2("wrc=%d, tag = %x\n",rc, tag);
                            DEBUG_0("Async write data completed ...\n");
                            pthread_mutex_unlock(&completion_lock);
                        } else if (rc == 0) {
                            pthread_mutex_lock(&completion_lock);
                            DEBUG_2("Waiting for command to complete wrc=%d, tag = %x\n",rc, tag);
                            pthread_mutex_unlock(&completion_lock);
                            usleep(1000);
                            continue;
                        } else {
                            pthread_mutex_lock(&completion_lock);
                            DEBUG_3("cblk_aresult completed (failed write) for  for tag = 0x%x, rc = %d, errno = %d\n",tag,rc,errno);
                            pthread_mutex_unlock(&completion_lock);
                        }
                        break;
                    } /* while */


                    rc = cblk_aread(blk_data->chunk_id[x],data_buf,blk_number,1,&tag,NULL,0);

                    if (rc < 0) {
                        blk_data->status.ret = rc;
                        blk_data->status.errcode = errno;
                        free(comp_data_buf);
                        free(data_buf);
                        pthread_mutex_lock(&completion_lock);
                        DEBUG_2("Aread failed rc = %d, errno = %d\n",rc, errno);
                        pthread_mutex_unlock(&completion_lock);
                        pthread_exit(&(blk_data->status));
                    } 

                    arflag = CBLK_ARESULT_BLOCKING;
                    while (TRUE) {

                        rc = cblk_aresult(blk_data->chunk_id[x],&tag, &ar_status,arflag);
                        if (rc > 0) {
                            pthread_mutex_lock(&completion_lock);
                            DEBUG_2("rc=%d, tag = %x\n",rc, tag);
                            DEBUG_0("Async read data completed ...\n");
                            pthread_mutex_unlock(&completion_lock);
                        } else if (rc == 0) {
                            pthread_mutex_lock(&completion_lock);
                            DEBUG_2("rc=%d, tag = %x\n",rc, tag);
                            DEBUG_3("Waiting for command to complete for tag = 0x%x, rc = %d, errno = %d\n",tag,rc,errno);
                            pthread_mutex_unlock(&completion_lock);
                            usleep(1000);
                            continue;
                        } else {
                            pthread_mutex_lock(&completion_lock);
                            DEBUG_3("cblk_aresult completed (failed read) for  for tag = 0x%x, rc = %d, errno = %d\n",tag,rc,errno);
                            pthread_mutex_unlock(&completion_lock);
                        }

                        break;

                    } /* while */

                    pthread_mutex_lock(&completion_lock);
                    DEBUG_2("Read ******** RC = %d tag %x\n",rc, tag);

                    DEBUG_1("Read completed with rc = %d\n",rc);
                    pthread_mutex_unlock(&completion_lock);

                    rc = memcmp(data_buf,comp_data_buf,BLK_FVT_BUFSIZE);

                    if (rc) {
                        blk_data->status.ret = rc;
                        blk_data->status.errcode = errno;
                        if (blk_verbosity==9) {
                            pthread_mutex_lock(&completion_lock);
                            fprintf(stderr,"Data compare failure\n");
                            /*
                               fprintf(stderr,"Written data:\n");
                               dumppage(data_buf,BLK_FVT_BUFSIZE);
                               fprintf(stderr,"**********************************************************\n\n");
                               fprintf(stderr,"read data:\n");
                               dumppage(comp_data_buf,BLK_FVT_BUFSIZE);
                               fprintf(stderr,"**********************************************************\n\n");
                             */
                            pthread_mutex_unlock(&completion_lock);
                        }	
                        rc = cblk_read(blk_data->chunk_id[x],data_buf,blk_number,1,0);

                        if (rc == 1) {
                            if (blk_verbosity==9) {
                                DEBUG_0("Dump re-read OK\n");
                                /*
                                   dumppage(data_buf,BLK_FVT_BUFSIZE);
                                 */
                            }
                        }

                    } else if (!thread_flag) {
                        DEBUG_0("Memcmp succeeded\n");
                    }
                    free(comp_data_buf);
                    break;
                default:

                    fprintf(stderr,"Invalid cmd_type = %d\n",cmd_type);
                    i = num_loops;
            } /* switch */


            blk_number++;

            pthread_mutex_lock(&completion_lock);
            DEBUG_3("Dev = %d. Loop count = %d of %d\n",x, i,num_loops);
            pthread_mutex_unlock(&completion_lock);
        } /* for num_loops */
    } /* for num_luns */

    free(data_buf);

    pthread_exit(&(blk_data->status));
}

void blk_thread_tst(int *ret, int *err)
{


    int rc;           /* Return code                 */
    int ret_code=0;
    int i,x;
    void            *status;
    blk_thread_status_t thread_stat;
    chunk_ext_arg_t ext = 0;
    int flags = 0;


    for (x=0; x < num_devs; x++) {
        blk_data.size = 64;
        if (virt_lun_flags)
            flags = CBLK_OPN_VIRT_LUN;
        if (share_cntxt_flags)
            flags |= CBLK_OPN_SHARE_CTXT;
        blk_data.chunk_id[x] = cblk_open(dev_paths[x],64,O_RDWR,ext,flags);

        if (blk_data.chunk_id[x] == NULL_CHUNK_ID) {

            DEBUG_2("Open of %s failed with errno = %d\n",dev_paths[x],errno);
            *ret = -1;
            *err = errno;
            return;
        }

        num_opens ++;
        /* Skip for physical luns */
        if (virt_lun_flags ) {

            /*
             * On the first pass thru this loop
             * for virtual lun, open the virtual lun
             * and set its size. Subsequent passes.
             * skip this step.
             */


            rc = cblk_set_size(blk_data.chunk_id[x],1024,0);

            if (rc) {
                perror("cblk_set_size failed\n");
                *ret = -1;
                *err = errno;
                for (x=0; x < num_opens; x++) {
                    cblk_close(blk_data.chunk_id[x], 0);
                }
                return;
            }

        }

    }
    if (num_threads >= 1) {

        /*
         * Create all threads here
         */

        for (i=0; i< num_threads; i++) {

            /*
               rc = pthread_create(&blk_thread[i].thread,NULL,blk_io_loop,(void *)&blk_data);
             */

            rc = pthread_create(&blk_thread[i],NULL,blk_io_loop,(void *)&blk_data);
            if (rc) {

                DEBUG_3("pthread_create failed for %d rc 0x%x, errno = 0x%x\n",
                        i, rc,errno);
                *ret = -1;
                *err = errno;
            }
        }


        /* 
         * Wait for all threads to complete
         */


        errno = 0;

        for (i=0; i< num_threads; i++) {

            rc = pthread_join(blk_thread[i],&status);

            thread_stat = *(blk_thread_status_t *)status;
            if(thread_stat.ret || thread_stat.errcode) {
                *ret = thread_stat.ret;
                *err = thread_stat.errcode;
                DEBUG_3("Thread %d returned fail ret %x, errno = %d\n",i,
                        thread_stat.ret,
                        thread_stat.errcode); 
            }
        }

    } 

    // fix close

    DEBUG_1("Calling cblk_close num open =%d...\n",num_opens);
    for (x=0; num_opens!=0; x++) {
        ret_code = cblk_close(blk_data.chunk_id[x],0);
        if (ret_code) {
            DEBUG_2("Close of %s failed with errno = %d\n",dev_paths[x],errno);
            *ret = ret_code;
            *err = errno;
        }
        num_opens --;
    }

}



void blocking_io_tst (chunk_id_t id, int *ret, int *err)
{
    int rc = -1;
    errno = 0;
    int cmdtag[1000];
    /*
       int artag[512];
     */
    int rtag = -1;
    uint64_t ar_status = 0;
    uint64_t lba;
    size_t nblocks=1;
    int fd, i;
    int arflg = 0;
    int t = 1;

    lba = 1;
    for (i=0; i < 1000; i++,lba++) {
        /* fill compare buffer with pattern */
        fd = open ("/dev/urandom", O_RDONLY);
        read (fd, blk_fvt_comp_data_buf, BLK_FVT_BUFSIZE);
        close (fd);
        rc = cblk_awrite(id,blk_fvt_comp_data_buf,lba,nblocks,&cmdtag[i],NULL,arflg);
        DEBUG_3("\n***** cblk_awrite rc = 0x%d, tag = %d, lba =0x%lx\n",rc, cmdtag[i], lba);
        if (!rc) {
            DEBUG_1("Async write returned tag = %d\n", cmdtag[i]);
        } else if (rc < 0) {
            DEBUG_3("awrite failed for  lba = 0x%lx, rc = %d, errno = %d\n",lba,rc,errno);
        } 
    }

    /*
       arflg = CBLK_ARESULT_NEXT_TAG|CBLK_ARESULT_BLOCKING;
     */
    arflg = CBLK_ARESULT_NEXT_TAG;

    while (TRUE) {
        rc = cblk_aresult(id,&rtag, &ar_status,arflg);

        if (rc > 0) {
            DEBUG_2("aresult rc = %d, tag = %d\n",rc,rtag);
            t++;
            if (t>1000)
                break;
        }
        if (rc == 0) {
            printf("Z");
            usleep(1000);
            continue;
        } else if (rc < 0) {
            DEBUG_1("aresult error = %d\n",errno);
            break;
        }
    }


    *ret = rc;
    *err = errno;

    return;
}


void io_perf_tst (chunk_id_t id, int *ret, int *err)
{
    int rc = -1;
    errno = 0;
    int cmdtag[20000];
    int rtag = -1;
    uint64_t ar_status = 0;
    uint64_t lba;
    size_t nblocks=1;
    int fd;
    int  i=0;
    int arflg = 0;
    int ret_code = 0;
    int t ;
    int x;
    int y = 0;
    char *env_num_cmds = getenv("FVT_NUM_CMDS");
    char *env_num_loop = getenv("FVT_NUM_LOOPS");
    char *env_io_comp = getenv("FVT_VALIDATE");


    int size = 4096;
    int loops = 500;
    int validate = 0;

    lba = 1;


    int num_cmds = 4096;

    if (env_num_cmds && atoi(env_num_cmds )) {
        num_cmds = atoi(env_num_cmds);
        /** limit 4K cmds **/
        if (num_cmds > 4096)
            num_cmds = 4096;
    }

    if (env_num_loop && atoi(env_num_loop )) {
        loops = atoi(env_num_loop);
    }

    if (env_io_comp && (atoi(env_io_comp)==1)) {
        validate = atoi(env_io_comp);
    }


    /* fill compare buffer with pattern */
    fd = open ("/dev/urandom", O_RDONLY);
    read (fd, blk_fvt_comp_data_buf, BLK_FVT_BUFSIZE*num_cmds);
    close (fd);

    for (x=0; x<loops ; x++, y++) {

        for (t=1, lba=1, i=0; i < num_cmds; i++,lba++) {
            rc = cblk_awrite(id,
                    (char*)(blk_fvt_comp_data_buf + (i*size)),
                    lba,nblocks,&cmdtag[i],NULL,arflg);
            if (rc < 0) {
                fprintf(stderr,"awrite failed for  lba = 0x%lx, rc = %d, errno = %d\n",lba,rc,errno);
                *ret = rc;
                *err = errno;
                return;
            } 
        }

        arflg = CBLK_ARESULT_NEXT_TAG;

        while (TRUE) {
            rc = cblk_aresult(id,&rtag, &ar_status,arflg);

            if (rc > 0) {
                t++;
                if (t>num_cmds)
                    break;
            }
            if (rc == 0)
                continue;
            if (rc < 0) {
                fprintf(stderr,"aresult cmdno = %d, error = %d, rc = 0x%x, \n",
                        t, errno,rc);
                *ret = rc;
                *err = errno;
                return;
            }
        }

        /* read wrote buffer */

        for (t=1, lba=1, i=0; i < num_cmds; i++,lba++) {
            rc = cblk_aread(id,
                    (char*)(blk_fvt_data_buf + (i*size)),
                    lba,nblocks,&cmdtag[i],NULL,arflg);
            if (rc < 0) {
                fprintf(stderr,"awrite failed for  lba = 0x%lx, rc = %d, errno = %d\n",lba,rc,errno);
                *ret = rc;
                *err = errno;
                return;
            } 
        }

        arflg = CBLK_ARESULT_NEXT_TAG;

        while (TRUE) {
            rc = cblk_aresult(id,&rtag, &ar_status,arflg);

            if (rc > 0) {
                t++;
                if (t>num_cmds)
                    break;
            }
            if (rc == 0)
                continue;
            if (rc < 0) {
                fprintf(stderr,"aresult error = %d\n",errno);
                *ret = rc;
                *err = errno;
                return;
            }
        }

        if (!validate) {
            ret_code = memcmp((char*)(blk_fvt_data_buf),
                    (char*)(blk_fvt_comp_data_buf),
                    BLK_FVT_BUFSIZE*size);

            if (ret_code) {
                fprintf(stderr,"\n memcmp failed rc = 0x%x\n",ret_code);
                *ret = ret_code;
                *err = errno;
                return;
            }
        }
    }
    DEBUG_2("Perf Test existing i = %d, x = %d\n", i, x );
    return;
}


int max_context(int *ret, int *err, int reqs, int cntx, int flags, int mode)
{

    int i               = 0;
    int t               = 0;
    chunk_id_t j        = NULL_CHUNK_ID;
    chunk_ext_arg_t ext = 0;
    errno               = 0;
    int status ;

    char *path          = dev_paths[0];

    pid_t	    child_pid [700];
    errno               = 0;

    if (test_max_cntx) {
        /* use user suppiled cntx value */
        cntx = test_max_cntx;
    }
    DEBUG_1("Testing max_cntx = %d\n", cntx);

    int         ok_to_close = 0;
    int         close_now = 0;
    int         pid;
    int         ret_pid;
    int         child_ret = 0;
    int         child_err = 0;
    int         ret_code,errcode = 0;
    int         fds[2*cntx];
    int         pipefds[2*cntx];

    // create pipe to be used by child  to pass back failure status
    for (i=0; i< (cntx); i++) {
        if (pipe(fds + (i*2) ) < 0 ) {
            perror ("pipe");
            fprintf(stderr, "\nIncrease numfile count with \"ulimit -n 5000\" cmd\n");
            *ret = -1;
            *err = errno;
            return(-1);
        }
    } 


    // create pipes to be used by child  to read permission to close
    for (i=0; i< (cntx); i++) {
        if (pipe(pipefds + (i*2) ) < 0 ) {
            perror ("pipe");
            fprintf(stderr, "\nIncrease numfile count with \"ulimit -n 5000\" cmd\n");
            *ret = -1;
            *err = errno;
            return(-1);
        }
    } 

    DEBUG_0("\nForking childrens to test  max contexts\n ");
    for (i = 0; i < cntx; i++) {
        DEBUG_2("Opening %s opn_cnt = %d\n", path,i);

        child_pid [i] = fork();

        if (child_pid [i] < 0 ) {
            fprintf(stderr,"\nmax_context: fork %d failed err=%d\n",i,errno);
            *ret = -1;
            *err = errno;
            return (-1);
        } else if (child_pid [i] == 0) {
            pid = getpid();
            j = cblk_open(path, reqs, mode, ext, flags);
            if (j != -1) {
                DEBUG_1("\nmax_context: Child = %d, OPENED \n", i+1 );
                close_now = 0;
                while ( !close_now) {
                    read(pipefds[i*2], &close_now, sizeof(int));
                    DEBUG_2("\nChild %d, Received %d \n",pid, close_now);
                }
                DEBUG_2("\nChild %d, Received Parent's OK =%d \n",pid, close_now);
                cblk_close(j, 0);
                /* exit success */
                exit(0);
            } else {
                fprintf(stderr,"\nmax_context: child  =%d ret = 0x%x,open error = %d\n",i+1, j, errno);	
                child_ret = j;
                child_err = errno;
                /* Send errcode thru ouput side */
                write(fds[(i*2)+1],&child_ret, sizeof(int));
                write(fds[(i*2)+1],&child_err, sizeof(int));

                /* exit error */
                exit(1);
            }
        } 
        DEBUG_1("\nmax_context: loops continue..opened = %d\n",i+1);
    }

    sleep (5);        
    for (i = 0; i < cntx; i++) {
        ok_to_close = 1;
        write(pipefds[(i*2)+1], &ok_to_close, sizeof(int));
        DEBUG_1("\nparent sends ok_to_close to %d \n",i+1);
    }
    ret_code = errcode = 0;
    /* Check all childs exited */
    for (i = 0; i < cntx; i++) {
        for (t = 0; t < 5; t++) {
            ret_pid = waitpid(child_pid[i], &status, 0);
            if (ret_pid == -1) {
                /* error */
                ret_code = -1;
                fprintf(stderr,"\nChild exited with error  %d \n",i);
                break;
            } else if (ret_pid == child_pid[i]) {
                DEBUG_1("\nChild exited  %d \n",i);
                if (WIFEXITED(status)) {
                    DEBUG_2("child =%d, status 0x%x \n",i,status);
                    if (WEXITSTATUS(status)) { 
                        read(fds[(i*2)], &ret_code, sizeof(ret_code));
                        read(fds[(i*2)], &errcode, sizeof(errcode));
                        DEBUG_3("\nchild %d errcode %d, ret_code %d\n",
                                i, errcode, ret_code);
                    } else {
                        DEBUG_1("child =%d, exited normally \n",i);
                    }
                }
                break;
            } else {
                if ( t == 4) {
                    errcode =ETIMEDOUT;
                    fprintf(stderr,"\nChild %d didn't exited Timed out\n",child_pid[i]);
                    break;
                }
                DEBUG_1("\nwaitpid returned = %d, give more time \n",ret_pid);
                DEBUG_1("\nChild %d give need more time \n",i);
                /* still running */
            }
            sleep (1);
        } /* give max 5 sec  */
        /* end test on any error */
        if (ret_code || errcode) {
            fprintf(stderr,"\nmax_context: Child = %d, ret = 0x%x, err = %d\n",
                    i+1, ret_code,errcode);
            break;
        }
    }

    *ret = ret_code;
    *err = errcode;

    // Close all pipes fds 

    for (i=0; i< 2*(cntx); i++) {
        close (pipefds [i]);
    }
    // Close all pipes fds 

    for (i=0; i< 2*(cntx); i++) {
        close (fds [i]);
    }

    return(0);
}

int child_open(int c, int max_reqs, int flags, int mode)
{
    chunk_id_t j = NULL_CHUNK_ID;
    chunk_ext_arg_t ext = 0;
    errno = 0;

    DEBUG_1 ("\nchild_open: opening for child %d\n", c);

    char *path          = dev_paths[0];

    j = cblk_open (path, max_reqs, mode, ext, flags);
    if (j != NULL_CHUNK_ID) {
        chunks[c] = j;
        num_opens += 1;
        return (0);
    } else {
        /*
         *id = j;
         *er_no = errno;
         */
        DEBUG_2("child_open: Failed: open i = %d, errno = %d\n", c, errno);
        return (-1);
    }
}
int fork_and_clone_mode_test(int *ret, int *err, int pmode, int cmode)
{
    chunk_id_t  id      = 0;
    int         flags   = CBLK_OPN_VIRT_LUN;
    int         sz_flags= 0;
    int         max_reqs= 64;
    int         open_cnt= 1;
    int         get_set_size_flag = 0;  // 0 = get phys lun sz
    // 1 = get chunk sz
    // 2 = set chunk sz

    int	    rc;
    int	    er;

    size_t      temp_sz;
    pid_t	    child_pid;
    int	    child_status;
    pid_t	    ret_pid;

    int         child_ret;
    int         child_err;
    int         ret_code=0;
    int         t;
    int         errcode = 0;
    int         fd[2];


    // create pipe to be used by child  to pass back status
    pipe(fd);

    if (blk_fvt_setup(1) < 0)
        return (-1);

    // open virtual  lun
    blk_open_tst( &id, max_reqs, &er, open_cnt, flags, pmode);

    if (id == NULL_CHUNK_ID)
        return (-1); 
    temp_sz = 64;
    get_set_size_flag = 2; 
    blk_fvt_get_set_lun_size(id, &temp_sz, sz_flags, get_set_size_flag, &rc, &er);
    if (rc | er) {
        DEBUG_0("fork_and_clone: set_size failed\n");
        *ret = rc;
        *err = er;
        return (0);
    }

    DEBUG_0("\nForking a child process ");
    child_pid = fork(); 
    if (child_pid < 0 ) {
        DEBUG_2("fork_and_clone: fork failed rc=%x,err=%d\n",rc,errno);
        *ret = -1;
        *err = errno;
        return (0);
    }
    if (child_pid == 0) {
        DEBUG_0("\nFork success,Running child process ");

        /* Close read side */
        close (fd[0]);
        // child process 
        rc = cblk_clone_after_fork(id,cmode,0);
        if (rc) {
            DEBUG_2("\nfork_and_clone: clone failed rc=%x, err=%d\n",rc, errno);
            child_ret = rc;
            child_err = errno;
            /* Send errcode thru ouput side */
            write(fd[1],&child_ret, sizeof(int));
            write(fd[1],&child_err, sizeof(int));
            DEBUG_1("Sending child_ret %d\n",child_ret);
            cblk_close(id,0);
            exit (1);
        }

        DEBUG_0("\nfork_and_clone: Exiting child process normally ");
        cblk_close(id,0);
        exit (0);

    } else {
        // parent's process
        DEBUG_0("\nfork_and_clone:Parent waiting for child proc ");
        ret_code = errcode = 0;
        for (t = 0; t < 5; t++) {
            ret_pid = waitpid(child_pid, &child_status, 0);
            if (ret_pid == -1) {
                *ret = -1;
                *err = errno;
                DEBUG_1("\nwaitpid error = %d \n",errno);
                return(0);
            } else if (ret_pid == child_pid) {
                DEBUG_0("\nChild exited Check child_status \n");
                if (WIFEXITED(child_status)) {
                    DEBUG_2("child =%d, child_status 0x%x \n",child_pid,child_status);
                    if (WEXITSTATUS(child_status)) { 
                        read(fd[0], &ret_code, sizeof(ret_code));
                        DEBUG_2("\nchild %d retcode %d\n",
                                child_pid, ret_code);
                        read(fd[0], &errcode, sizeof(errcode));
                        DEBUG_2("\nchild %d errcode %d\n",
                                child_pid, errcode);
                    } else {
                        DEBUG_1("child =%d, exited normally \n",child_pid);
                    }
                }
                break;
            } else {
                if ( t == 4) {
                    ret_code = -1;
                    errcode =ETIMEDOUT;
                    fprintf(stderr,"\nChild %d didn't exited Timed out\n",child_pid);
                    break;
                }
                fprintf(stderr,"\nwaitpid returned = %d, give more time \n",ret_pid);
                /* still running */
            }
            sleep (1);
        } /* give max 5 sec  */

        *ret = ret_code;
        *err = errcode;
    }

    return(0);
}

int fork_and_clone(int *ret, int *err, int mode)
{
    chunk_id_t  id      = 0;
    int         flags   = CBLK_OPN_VIRT_LUN;
    int         sz_flags= 0;
    int         max_reqs= 64;
    int         open_cnt= 1;
    int         get_set_size_flag = 0;  // 0 = get phys lun sz
    // 1 = get chunk sz
    // 2 = set chunk sz

    int	    rc;
    int	    er;

    uint64_t    lba;
    int         io_flags = 0;
    int	    open_flag = 0;
    size_t      temp_sz,nblks;
    int         cmd;
    pid_t	    child_pid;
    int	    child_status;
    pid_t	    w_ret;

    int         child_ret;
    int         child_err;
    int         ret_code=0;
    int         ret_err=0;
    int         fd[2];


    // create pipe to be used by child  to pass back status

    pipe(fd);

    if (blk_fvt_setup(1) < 0)
        return (-1);

    // open virtual  lun
    blk_open_tst( &id, max_reqs, &er, open_cnt, flags, mode);

    if (id == NULL_CHUNK_ID)
        return (-1); 
    temp_sz = 64;
    get_set_size_flag = 2; 
    blk_fvt_get_set_lun_size(id, &temp_sz, sz_flags, get_set_size_flag, &rc, &er);
    if (rc | er) {
        DEBUG_0("fork_and_clone: set_size failed\n");
        *ret = rc;
        *err = er;
        return (0);
    }

    cmd = FV_WRITE;
    lba = 1;
    nblks = 1;
    blk_fvt_io(id, cmd, lba, nblks, &rc, &er, io_flags, open_flag); 

    if (rc != 1) {
        DEBUG_0("fork_and_clone: blk_fvt_io failed\n");
        *ret = rc;
        *err = er;
        return (0);
    }

    DEBUG_0("\nForking a child process ");
    child_pid = fork(); 
    if (child_pid < 0 ) {
        DEBUG_0("fork_and_clone: fork failed\n");
        *ret = -1;
        *err = errno;
        return (0);
    }
    if (child_pid == 0) {
        DEBUG_0("\nFork success,Running child process ");

        /* Close read side */
        close(fd[0]);

        // child process 
        rc = cblk_clone_after_fork(id,O_RDONLY,0);
        if (rc) {

            DEBUG_2("\nfork_and_clone: clone failed rc=%x, err=%d\n",rc, errno);
            child_ret = rc;
            child_err = errno;
            /* Send errcode thru ouput side */
            write(fd[1],&child_ret, sizeof(int));
            write(fd[1],&child_err, sizeof(int));
            DEBUG_2("Sending child ret=%d, err=%d\n",child_ret, child_err);
            cblk_close(id,0);
            exit (1);
        }

        cmd = FV_READ;
        lba = 1;
        nblks = 1;
        blk_fvt_io(id, cmd, lba, nblks, &rc, &er, io_flags, open_flag); 
        if (rc != 1) {
            // error
            DEBUG_0("fork_and_clone: child I/O failed\n");
            child_ret = rc;
            child_err = errno;
            /* Send errcode thru ouput side */
            write(fd[1],&child_ret, sizeof(int));
            write(fd[1],&child_err, sizeof(int));
            DEBUG_2("Sending child ret=%d, err=%d\n",child_ret, child_err);
            cblk_close(id,0);
            exit (1);
        }
        blk_fvt_cmp_buf(nblks, &rc);
        if (rc) {
            // error
            DEBUG_0("fork_and_clone: child I/O compare failed\n");
            child_ret = rc;
            child_err = errno;
            /* Send errcode thru ouput side */
            write(fd[1],&child_ret, sizeof(int));
            write(fd[1],&child_err, sizeof(int));
            DEBUG_2("Sending child ret=%d, err=%d\n",child_ret, child_err);
            cblk_close(id,0);
            exit (1);
        }
        DEBUG_1("fork_and_clone: Child buf compare ret rc = %d\n",rc);
        DEBUG_0("\nfork_and_clone: Exiting child process normally ");
        cblk_close(id,0);
        exit (0);

    } else {
        // parent's process

        DEBUG_0("\nfork_and_clone:Parent waiting for child proc ");
        w_ret = wait(&child_status);

        if (w_ret == -1) {
            DEBUG_1("fork_and_clone: wait failed %d\n",errno);
            *ret = -1;
            *err = errno;
            return (0);
        } else {

            DEBUG_0("\nfork_and_clone: Child process returned ");
            if (WIFEXITED(child_status)) {
                DEBUG_1("\nfork_and_clone:1 child exit status = 0x%x\n",child_status);
                if (WEXITSTATUS(child_status)) {
                    DEBUG_1("\nfork_and_clone: Error child exit status = 0x%x\n",child_status);
                    /* Close output side */
                    close(fd[1]);
                    rc = read(fd[0], &ret_code, sizeof(ret_code));
                    DEBUG_1("Received child status %d\n",ret_code);
                    rc = read(fd[0], &ret_err, sizeof(ret_err));
                    DEBUG_1("Received child errcode=%d \n",ret_err);
                    *ret = ret_code;
                    *err = ret_err;
                } else {
                    DEBUG_1("\nfork_and_clone: Successfullchild exit status = 0x%x\n",child_status);
                    *ret = rc;
                    *err = errno;
                }
            }
        }
    }

    return(0);
}

void blk_list_io_arg_test(chunk_id_t id, int arg_tst, int *err, int *ret)
{
    int 	rc = 0;
    int		i;
    int		num_complt;
    uint64_t	timeout = 0;
    uint64_t	lba = 1;
    int		uflags = 0;
    size_t	size = 1;

    cblk_io_t	cblk_issue_io[1];
    cblk_io_t	*cblk_issue_list[1];
    cblk_io_t	cblk_complete_io[1];
    cblk_io_t	*cblk_complete_list[1];

    cblk_io_t	cblk_wait_io[1];
    cblk_io_t	*cblk_wait_list[1];

    cblk_io_t	cblk_pending_io[1];

    num_complt = 1;

    // allocate buffer for 1 IO

    for (i=0; i<1; i++) {  
        bzero(&cblk_issue_io[i],sizeof(cblk_io_t));
        bzero(&cblk_complete_io[i],sizeof(cblk_io_t)); 
        bzero(&cblk_wait_io[i],sizeof(cblk_io_t));
        bzero(&cblk_pending_io[i],sizeof(cblk_io_t));
    }
    i = 0;
    lba = 1;
    if (arg_tst == 9)
        uflags = CBLK_IO_USER_STATUS;
    else
        uflags = 0;
    size = 1;
    cblk_issue_io[i].request_type = CBLK_IO_TYPE_WRITE;
    cblk_issue_io[i].buf = (void *)(blk_fvt_comp_data_buf );
    cblk_issue_io[i].lba = lba;
    cblk_issue_io[i].flags = uflags;
    cblk_issue_io[i].nblocks = size;
    cblk_issue_list[i] = &cblk_issue_io[i];

    cblk_complete_list[i] = &cblk_complete_io[i];

    cblk_wait_io[i].request_type = CBLK_IO_TYPE_WRITE;
    cblk_wait_io[i].buf = (void *)(blk_fvt_comp_data_buf);
    cblk_wait_io[i].lba = lba;
    cblk_wait_io[i].flags = uflags;
    cblk_wait_io[i].nblocks = size;
    cblk_wait_list[i] = &cblk_wait_io[i];

    cblk_pending_io[i].request_type = CBLK_IO_TYPE_WRITE;
    cblk_pending_io[i].buf = (void *)(blk_fvt_comp_data_buf);
    cblk_pending_io[i].lba = lba;
    cblk_pending_io[i].flags = uflags;
    cblk_pending_io[i].nblocks = size;


    errno = 0;
    switch (arg_tst) {
        case 1:
            /* ALL list NULL */
            rc = cblk_listio(id, 
                    NULL,0, 
                    NULL,0, 
                    NULL,0, 
                    NULL,&num_complt, 
                    timeout,0);
            break;
        case 2:
            rc = cblk_listio(id, 
                    cblk_issue_list, 0, 
                    NULL,0, 
                    NULL,0, 
                    NULL,&num_complt, 
                    timeout,0);
            break;
        case 3: 
            rc = cblk_listio(id, 
                    cblk_issue_list, 1, 
                    NULL,0, 
                    NULL,0, 
                    cblk_complete_list,0, 
                    timeout,0);
            break;
        case 4: 
            rc = cblk_listio(id, 
                    cblk_issue_list, 1, 
                    NULL,0, 
                    NULL,0, 
                    NULL,&num_complt, 
                    timeout,0);
            break;
        case 5: 
            rc = cblk_listio(id, 
                    NULL,0, 
                    NULL,0, 
                    NULL,0, 
                    cblk_complete_list,&num_complt, 
                    timeout,0);
            break;
        case 6: 
            /* Test null buffer report EINVAL error */
            cblk_issue_io[i].buf = (void *)NULL;
            rc = cblk_listio(id, 
                    cblk_issue_list, 1, 
                    NULL,0, 
                    cblk_wait_list,1, 
                    cblk_complete_list,&num_complt, 
                    timeout,0);
            break;
        case 7: 
            /* Test null num_complt report EINVAL */
            num_complt = 0;
            rc = cblk_listio(id, 

                    cblk_issue_list, 1, 
                    NULL,0, 
                    NULL,1, 
                    cblk_complete_list,&num_complt, 
                    timeout,0);
            break;
        case 8: 
            /* Test -1 num_complt report EINVAL */
            num_complt = -1;
            rc = cblk_listio(id, 
                    cblk_issue_list, 1, 
                    NULL,0, 
                    NULL,1, 
                    cblk_complete_list,&num_complt, 
                    timeout,0);
            break;
        case 9: 
            /* Test T/O with CBLK_USER_STATUS  report EINVAL */
            timeout = 1;
            rc = cblk_listio(id, 
                    cblk_issue_list, 1, 
                    NULL,0, 
                    NULL,1, 
                    cblk_complete_list,&num_complt, 
                    timeout,0);
            break;
        default:
            break;
    }

    DEBUG_1("Invalid arg test %d Complete\n",arg_tst);
    *ret = rc;
    *err = errno;
    return;
}

void blk_list_io_test(chunk_id_t id, int cmd, int t_type, int uflags, uint64_t timeout, int *err, int *ret, int num_listio)
{
    int rc = 0;
    int i;
    uint64_t lba;
    size_t  size = 1;
    int ret_code = 0;
    int num_complt = num_listio;
    int cmplt_cnt = 0;

    cblk_io_t cblk_issue_io[num_listio];
    cblk_io_t *cblk_issue_list[num_listio];
    cblk_io_t cblk_complete_io[num_listio];
    cblk_io_t *cblk_complete_list[num_listio];

    cblk_io_t cblk_wait_io[num_listio];
    cblk_io_t *cblk_wait_list[num_listio];

    cblk_io_t cblk_pending_io[num_listio];
    cblk_io_t *cblk_pending_list[num_listio];


    for (i=0; i<num_listio; i++) {  
        bzero(&cblk_issue_io[i],sizeof(cblk_io_t));
        bzero(&cblk_complete_io[i],sizeof(cblk_io_t)); 
        bzero(&cblk_wait_io[i],sizeof(cblk_io_t));
        bzero(&cblk_pending_io[i],sizeof(cblk_io_t));
    }

    // create list of cmd I/O

    for (i=0,lba=1; i<num_listio; i++,lba++) {
        if (cmd == FV_WRITE) {
            cblk_issue_io[i].request_type = CBLK_IO_TYPE_WRITE;
            cblk_issue_io[i].buf = (char *)(blk_fvt_comp_data_buf + (i*BLK_FVT_BUFSIZE));
        } else {
            cblk_issue_io[i].request_type = CBLK_IO_TYPE_READ;
            cblk_issue_io[i].buf = (void *)(blk_fvt_data_buf + (i*BLK_FVT_BUFSIZE));
        }
        cblk_issue_io[i].lba = lba;
        cblk_issue_io[i].flags = uflags;
        cblk_issue_io[i].nblocks = size;

        cblk_issue_list[i] = &cblk_issue_io[i];
        cblk_complete_list[i] = &cblk_complete_io[i];
    }

    // create list of wait I/O
    for (i=0,lba=1; i<num_listio; i++,lba++) {
        if (cmd == FV_WRITE) {
            cblk_wait_io[i].request_type = CBLK_IO_TYPE_WRITE;
            cblk_wait_io[i].buf = (void *)(blk_fvt_comp_data_buf + (i*BLK_FVT_BUFSIZE));
        } else {
            cblk_wait_io[i].request_type = CBLK_IO_TYPE_READ;
            cblk_wait_io[i].buf = (void *)(blk_fvt_data_buf + (i*BLK_FVT_BUFSIZE));
        }
        cblk_wait_io[i].lba = lba;
        cblk_wait_io[i].flags = uflags;
        cblk_wait_io[i].nblocks = size;

        cblk_wait_list[i] = &cblk_wait_io[i];
    }

    // create list of pending I/O
    for (i=0,lba=1; i<num_listio; i++,lba++) {
        if (cmd == FV_WRITE) {
            cblk_pending_io[i].request_type = CBLK_IO_TYPE_WRITE;
            cblk_pending_io[i].buf = (void *)(blk_fvt_comp_data_buf + (i*BLK_FVT_BUFSIZE));
        } else {
            cblk_pending_io[i].request_type = CBLK_IO_TYPE_READ;
            cblk_pending_io[i].buf = (void *)(blk_fvt_data_buf + (i*BLK_FVT_BUFSIZE));
        }
        cblk_pending_io[i].lba = lba;
        cblk_pending_io[i].flags = uflags;
        cblk_pending_io[i].nblocks = size;

        cblk_pending_list[i] = &cblk_pending_io[i];
    }


    DEBUG_1("Issue listio cmd = %d\n",cmd);

    if ((uflags & CBLK_IO_USER_STATUS) && !timeout) {
        /* issue listio and poll issue list for completions */
        rc = cblk_listio(id, 
                cblk_issue_list, num_listio, 
                NULL,0, 
                NULL,0, 
                cblk_complete_list,&num_complt, 
                timeout,0);
    } else if (t_type == 2) {
        /* USER_STATUS _not_ set */
        /* issue listio and poll waitlist for completions */
        rc = cblk_listio(id, 
                cblk_issue_list, num_listio, 
                NULL,0, 
                cblk_wait_list, num_listio, 
                cblk_complete_list,&num_complt, 
                timeout,0);
    } else if (t_type == 3){
        /* USER_STATUS _not_ set */
        /* Issue listio with issue and compl list, followed by pending list 
         * and completion with num_complt
         */
        rc = cblk_listio(id, 
                cblk_issue_list, num_listio, 
                NULL,0, 
                NULL, 0, 
                cblk_complete_list,&num_complt, 
                timeout,0);
        if (!rc) {
            num_complt = num_listio;

            bcopy (&cblk_issue_io[0], &cblk_pending_io[0], ((sizeof(cblk_io_t))*num_listio));
            while (TRUE) {
                /* Issue listio  with pending and completion list , and then
                 * check for completion list for cmds being complete,
                 * Re-issue listio with pending lists
                 */
                rc = cblk_listio(id, 
                        NULL,0, 
                        cblk_pending_list, num_listio, 
                        NULL, 0, 
                        cblk_complete_list,&num_complt, 
                        timeout,0);

                if (!rc) {
                    /* returns num compltions */
                    ret_code = check_completions(&cblk_complete_io[0],num_listio);
                    /* returns num complts. or -1 for error*/
                    /* if all complete or any failed , exit */
                    if (ret_code == -1) {
                        rc = ret_code;
                        fprintf(stderr, "List io failed cmplt_cnt =%d\n",cmplt_cnt);
                        break;
                    } else {
                        cmplt_cnt += ret_code;
                        if (cmplt_cnt == num_listio) {
                            rc = 0;
                            DEBUG_1("\ncmplt_cnt = %d\n",cmplt_cnt);
                            break;
                        }
                    }
                    /* re-issue with pending count */
                    num_complt = num_listio;
                } else {
                    fprintf(stderr, "Type 3 listio test failed, errno =0x%x\n",errno);
                    break;
                }
            }
        } else {
            fprintf(stderr,"cblk_listio cmd = %d failed , compl_items =%d, rc = %d, errno = %d\n",cmd, cmplt_cnt, rc, errno);
            *ret = rc;
            *err = errno;
            return ;
        }
    }

    if (!rc) {
        if (uflags & CBLK_IO_USER_STATUS) {
            DEBUG_1("\nPolling issue io for cmd type = %d \n",cmd);
            /* if caller is providing a status location */
            rc = poll_arw_stat(&cblk_issue_io[0],num_listio); 
        } else if (t_type == 2) {
            DEBUG_1("\nPolling wait io for cmd type = %d \n",cmd);
            rc = poll_arw_stat(&cblk_wait_io[0],num_listio); 

        }
    }
    if (rc == -1) {
        fprintf(stderr,"listio cmd = %d failed rc = %x, err = %x\n", cmd, rc, errno);
        if ((errno == ETIMEDOUT) && (t_type == 2)) {
            /* This force 1 micro sec t/o test, so wait for cmd completion */
            poll_arw_stat(&cblk_wait_io[0],num_listio); 
        }
    }
    DEBUG_1("\nlistio cmd type = %d complete\n",cmd);

    *ret = rc;
    *err = errno; 
    return;
}

int poll_arw_stat(cblk_io_t *io, int num_listio)
{

    int i=0;
    int cmds_fail,cmds_invalid,cmds_cmplt;
    cmds_fail=cmds_invalid=cmds_cmplt=0;
    int ret = -1;
    int sleep_cnt = 0;

    DEBUG_0("Poll completions ...\n");
    DEBUG_2("Enetering poll_arw_stat: , slept = %d, all cmds %d done\n",sleep_cnt, cmds_cmplt);

    while (TRUE) {
        for (i=0; i<num_listio; i++) {
            switch (io[i].stat.status) {
                case CBLK_ARW_STATUS_FAIL:
                    fprintf(stderr," %d req failed, err %d, xfersize =0x%lx\n",
                        i,io[i].stat.fail_errno, io[i].stat.blocks_transferred);
                    cmds_fail ++;
                    /* mark it counted */
                    io[i].stat.status = -1;
                    errno = io[i].stat.fail_errno ;
                    break;
                case CBLK_ARW_STATUS_PENDING:
                    DEBUG_3(" %d req pending, err %d, xfersize =0x%lx\n",
                        i,io[i].stat.fail_errno, io[i].stat.blocks_transferred);
                    break;
                case CBLK_ARW_STATUS_SUCCESS:
                    DEBUG_3("\n %d req success, err %d, xfersize =%lx\n\n",
                        i,io[i].stat.fail_errno, io[i].stat.blocks_transferred);
                    cmds_cmplt ++;
                    /* mark it counted */
                    io[i].stat.status = -1;
                    break;
                case CBLK_ARW_STATUS_INVALID:
                    fprintf(stderr," %d req invalid, err %d, xfersize =%lx\n",
                        i,io[i].stat.fail_errno, io[i].stat.blocks_transferred);
                    cmds_invalid ++;
                    break;
                default:
                    /* if this is marked counted */
                    if ((io[i].stat.status) == -1)
                        break;
                    else
                        fprintf(stderr,"Invalid status %x\n",
                                            (io[i].stat.status)); 
                    break;
            }

        }
        if (cmds_fail ) {
            if ((cmds_fail + cmds_cmplt) == num_listio) {
                fprintf(stderr, "Exiting poll_arw_stat with erreno\n");
                return (ret);
            }
        }
        if ( cmds_cmplt == num_listio) {
            DEBUG_2("Exiting poll_arw_stat: , slept = %d, all cmds %d done\n",
                            sleep_cnt, cmds_cmplt);
            ret = 0;
            return(ret);
        }
        if (sleep_cnt > 50) {
            fprintf(stderr, "Waited 50 sec, fail = %d, cmplt =%d\n",
                            cmds_fail,cmds_cmplt);
            break;
        }
        sleep_cnt ++;
        sleep(1);
    }

    return (ret);

}

int check_completions(cblk_io_t *io, int num_listio )
{
    int i;
    int cmds_fail,cmds_invalid,cmds_cmplt,cmds_pend;
    cmds_fail=cmds_invalid=cmds_cmplt=cmds_pend=0;


    for (i=0; i<num_listio; i++) {
        switch (io[i].stat.status) {
            case CBLK_ARW_STATUS_FAIL:
                fprintf(stderr," %d req failed, err %d, xfersize =%lx\n",
                    i,io[i].stat.fail_errno, io[i].stat.blocks_transferred);
                cmds_fail ++;
                // mark it counted
                io[i].stat.status = -1;
                errno = io[i].stat.fail_errno ;
                break;
            case CBLK_ARW_STATUS_PENDING:
                DEBUG_3(" %d req pending, err %d, xfersize =0x%lx\n",
                    i,io[i].stat.fail_errno, io[i].stat.blocks_transferred);
                cmds_pend ++;
                break;
            case CBLK_ARW_STATUS_SUCCESS:
                DEBUG_3(" %d req success, err %d, xfersize =%lx\n",
                    i,io[i].stat.fail_errno, io[i].stat.blocks_transferred);
                cmds_cmplt ++;
                // mark it counted
                io[i].stat.status = -1;
                break;
            case CBLK_ARW_STATUS_INVALID:
                fprintf(stderr," %d req invalid, err %d, xfersize =%lx\n",
                       i,io[i].stat.fail_errno, io[i].stat.blocks_transferred);
                cmds_invalid ++;
                break;
            default:
                /*
                   fprintf(stderr, "Invalid status\n"); 
                 */
                break;
        }

    }
    if (cmds_fail) {
        fprintf(stderr, "pend = %d, fail = %d, cmplt =%d\n",
                    cmds_pend, cmds_fail,cmds_cmplt);
        return (-1);
    } 
    if (cmds_cmplt ) {
        DEBUG_3("pend = %d, fail = %d, cmplt =%d\n",
                        cmds_pend, cmds_fail,cmds_cmplt);
        return(cmds_cmplt); 
    }

    return (0);

}


#ifdef _AIX
char *find_parent(char *device_name)
{

    return NULL;
}
char *getunids (char *device_name)
{

    return NULL;
}
#else

char *find_parent(char *device_name)
{
    char *parent_name = NULL;
    char *child_part = NULL;
    const char *device_mode = NULL;
    char *subsystem_name = NULL;
    char *devname = NULL;


    struct udev *udev_lib;
    struct udev_device *device, *parent;


    udev_lib = udev_new();

    if (udev_lib == NULL) {
        DEBUG_1("udev_new failed with errno = %d\n",errno);
        fprintf(stderr,"udev_open failed\n");
        return parent_name;
    }

    /*
     * Extract filename with absolute path removed
     */
    devname = rindex(device_name,'/');
    if (devname == NULL) {
        devname = device_name;
    } else {
        devname++;
        if (devname == NULL) {
            DEBUG_1("\ninvalid device name = %s",devname);
            return parent_name;
        }
    }

    if (!(strncmp(devname,"sd",2))) {
        subsystem_name = "block";
    } else if (!(strncmp(devname,"sg",2))) {
        subsystem_name = "scsi_generic";
    } else {
        DEBUG_1("\ninvalid device name = %s",devname);
        return parent_name;
    }


    device = udev_device_new_from_subsystem_sysname(udev_lib,subsystem_name,devname);
    if (device == NULL) {
        DEBUG_1("\n udev_device_new_from_subsystem_sysname failed with errno = %d",errno);
        return parent_name;
    }

    device_mode = udev_device_get_sysattr_value(device,"mode");
    if (device_mode == NULL) {

        DEBUG_0("\nno mode for this device ");
        // return parent_name;
    }
    parent =  udev_device_get_parent(device);
    if (parent == NULL) {
        DEBUG_1("\n udev_device_get_parent failed with errno = %d",errno);
        return parent_name;
    }

    parent_name = (char *)udev_device_get_devpath(parent);


    /*
     * This parent name string will actually have sysfs directories
     * associated with the connection information of the child. We
     * need to get the base name of parent device that is not associated with
     * child device.  Find child portion of parent name string
     * and insert null terminator there to remove it.
     */

    child_part = strstr(parent_name,"/host");
    if (child_part) {
        child_part[0] = '\0';
    }
    return parent_name;
}

#endif /* !AIX */
