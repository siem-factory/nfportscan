/*
 *  This file is part of the nfdump project.
 *
 *  Copyright (c) 2004, SWITCH - Teleinformatikdienste fuer Lehre und Forschung
 *  All rights reserved.
 *  
 *  Redistribution and use in source and binary forms, with or without 
 *  modification, are permitted provided that the following conditions are met:
 *  
 *   * Redistributions of source code must retain the above copyright notice, 
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright notice, 
 *     this list of conditions and the following disclaimer in the documentation 
 *     and/or other materials provided with the distribution.
 *   * Neither the name of SWITCH nor the names of its contributors may be 
 *     used to endorse or promote products derived from this software without 
 *     specific prior written permission.
 *  
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 *  POSSIBILITY OF SUCH DAMAGE.
 *  
 *  $Author: peter $
 *
 *  $Id: util.h 95 2007-10-15 06:05:26Z peter $
 *
 *  $LastChangedRevision: 95 $
 *	
 */


#define FILE_ERROR -1
#define EMPTY_LIST -2

#define EBUFF_SIZE 256

#ifdef WORDS_BIGENDIAN
#	define ntohll(n)	(n)
#	define htonll(n)	(n)
#else
#	define ntohll(n)	(((uint64_t)ntohl(n)) << 32) + ntohl((n) >> 32)
#	define htonll(n)	(((uint64_t)htonl(n)) << 32) + htonl((n) >> 32)
#endif

typedef struct stringlist_s {
	uint32_t	block_size;
	uint32_t	max_index;
	uint32_t	num_strings;
	char		**list;
} stringlist_t;

void InitStringlist(stringlist_t *list, int block_size);

void InsertString(stringlist_t *list, char *string);

int ScanTimeFrame(char *tstring, time_t *t_start, time_t *t_end);

char *TimeString(time_t start, time_t end);

char *UNIX2ISO(time_t t);

time_t ISO2UNIX(char *timestring);

void SetupInputFileSequence(char *multiple_dirs, char *single_file, char *multiple_files);

char *GetCurrentFilename(void);

void Setv6Mode(int mode);

