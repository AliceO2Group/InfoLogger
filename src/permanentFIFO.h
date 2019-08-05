// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#ifdef __cplusplus
extern "C" {
#endif

/**************************************/
/* implementation of a permanent FIFO */
/**************************************/

struct permFIFO;

/* struct to store a FIFO item in memory */
struct FIFO_item {
  unsigned long size; /* size of data */
  void* data;         /* pointer to data, allocated with checked_malloc(), to be freed with checked_free() */
  unsigned long id;   /* id of the FIFO item, used to ACK. Reset to 0 when FIFO empty */
};

/** Constructor */
struct permFIFO* permFIFO_new(int size, char* path);

/** Destructor */
int permFIFO_destroy(struct permFIFO* f);

/** Read FIFO */
int permFIFO_read(struct permFIFO* f, struct FIFO_item* item, int timeout);

/** Write FIFO */
int permFIFO_write(struct permFIFO* f, void* data, int size);

/** Remove all items which have lower id than the one given */
int permFIFO_ack(struct permFIFO* f, unsigned long id);

/** Flush data to disk, if timeout elapsed since last flush (use 0 to force, timeout is in seconds) */
int permFIFO_flush(struct permFIFO* f, int timeout);

#ifdef __cplusplus
}
#endif
