/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: file.h
Version: 0.6.7

StaRev is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3
as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.
===========================================================================
*/

/*
   Copyright (c) 2001, Loki software, inc.
   All rights reserved.

   Redistribution and use in source and binary forms, with or without modification,
   are permitted provided that the following conditions are met:

   Redistributions of source code must retain the above copyright notice, this list
   of conditions and the following disclaimer.

   Redistributions in binary form must reproduce the above copyright notice, this
   list of conditions and the following disclaimer in the documentation and/or
   other materials provided with the distribution.

   Neither the name of Loki software nor the names of its contributors may be used
   to endorse or promote products derived from this software without specific prior
   written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
   DIRECT,INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
   ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

//
//	file.h
////////////////////////////////////////////////////

#ifndef _FILE_H_
#define _FILE_H_

//#include <stdio.h>

class MemStream : public IDataStream
{
public:
  MemStream();
  MemStream( unsigned long nLen );
  virtual ~MemStream();

  int refCount;
  void IncRef() { refCount++; }
  void DecRef() {
    refCount--; if ( refCount <= 0 ) {
      delete this;
    }
  }

protected:
  // MemFile specific:
  unsigned long m_nGrowBytes;
  unsigned long m_nPosition;
  unsigned long m_nBufferSize;
  unsigned long m_nFileSize;
  unsigned char* m_pBuffer;
  bool m_bAutoDelete;
  void GrowFile( unsigned long nNewLen );

public:
  unsigned long GetPosition() const;
  unsigned long Seek( long lOff, int nFrom );
  void SetLength( unsigned long nNewLen );
  unsigned long GetLength() const;

  unsigned char* GetBuffer() const
  { return m_pBuffer; }

  char* ReadString( char* pBuf, unsigned long nMax );
  unsigned long Read( void* pBuf, unsigned long nCount );
  unsigned long Write( const void* pBuf, unsigned long nCount );
  int GetChar();
  int PutChar( int c );

  void printf( const char*, ... ); ///< \todo implement on MemStream

  void Abort();
  void Flush();
  void Close();
  bool Open( const char *filename, const char *mode );
};

class FileStream : public IDataStream
{
public:
  FileStream();
  virtual ~FileStream();

  int refCount;
  void IncRef() { refCount++; }
  void DecRef() {
    refCount--; if ( refCount <= 0 ) {
      delete this;
    }
  }

protected:
  // DiscFile specific:
  FILE* m_hFile;
  bool m_bCloseOnDelete;

  public:
  unsigned long GetPosition() const;
  unsigned long Seek( long lOff, int nFrom );
  void SetLength( unsigned long nNewLen );
  unsigned long GetLength() const;

  char* ReadString( char* pBuf, unsigned long nMax );
  unsigned long Read( void* pBuf, unsigned long nCount );
  unsigned long Write( const void* pBuf, unsigned long nCount );
  int GetChar();
  int PutChar( int c );

  void printf( const char*, ... ); ///< completely matches the usual printf behaviour

  void Abort();
  void Flush();
  void Close();
  bool CloseChecked();
  bool Open( const char *filename, const char *mode );
};

#endif // _FILE_H_
