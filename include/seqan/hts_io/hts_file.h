#ifndef SEQAN_HTS_IO_HTS_FILE_IN_H_
#define SEQAN_HTS_IO_HTS_FILE_IN_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <sys/types.h>

#include <seqan/basic.h>

#include <htslib/hfile.h>
#include <htslib/hts.h>
#include <htslib/sam.h>
// #include <htslib/vcf.h>
#include <htslib/bgzf.h>

#include <seqan/hts_io/bam_alignment_record.h>
#include <seqan/hts_io/hts_alignment_record.h>


namespace seqan
{

class HtsFile
{
  public:
    const char * filename;  /** @brief The filename of the current file. */
    htsFile * fp;           /** @brief Pointer to the file. */
    bam_hdr_t * hdr;        /** @brief The header of the current file. */
    bam1_t * hts_record;    /** @brief The current HTS record. */
    hts_idx_t * hts_index;  /** @brief The index of the file. */
    hts_itr_t * hts_iter;   /** @brief An iterator that iterates through a certain region in the HTS file. */
    const char * file_mode; /** @brief Which file mode to use. E.g. "r" for reading and "wb" for writing binaries. */
    bool at_end = false;

    /**
     * @brief Empty HTS file constructor
     */
    HtsFile(const char * mode = "r")
      : filename(""), fp(nullptr), hdr(nullptr), hts_record(nullptr), hts_index(nullptr), hts_iter(nullptr), file_mode(mode), at_end(false) {}

    /**
     * @brief Constructs a new HtsFile object.
     * 
     * @param f The filename of the file.
     * @param mode The file mode to use when opening the file.
     * @return A new HtsFile object.
     */
    HtsFile(const char * f, const char * mode)
      : filename(f), fp(nullptr), hdr(nullptr), hts_record(nullptr), hts_index(nullptr), hts_iter(nullptr), file_mode(mode), at_end(false)
    {
        open();
    }

    /**
     * @brief Destructs an HtsFile object.
     */
    ~HtsFile()
    {
        bam_hdr_destroy(hdr);
        hts_close(fp);
    }

    inline bool
    open()
    {
        static const char * read_mode = "r";
        fp = hts_open(filename, file_mode);

        if (fp == nullptr)
        {
            // return false;
            SEQAN_FAIL("Could not open file with filename %s", filename);
        }

        if (file_mode == read_mode)
        {
            hdr = sam_hdr_read(fp);
        }

        hts_record = bam_init1();
        return true;
    }
};

class HtsFileIn : public HtsFile
{
  public:
    HtsFileIn()
      : HtsFile("r") {}

    HtsFileIn(const char * f)
      : HtsFile(f, "r") {}
};

class HtsFileOut : public HtsFile
{
  public:
    HtsFileOut()
      : HtsFile("wb") {}

    HtsFileOut(const char * f)
      : HtsFile(f, "wb") {}
};


/* For backwards compability */
typedef HtsFileIn BamFileIn;
typedef HtsFileOut BamFileOut;


/**
 * @brief Opens a HTS file from filename.
 * 
 * @param target An empty HtsFile object.
 * @param f The filename to open from.
 */
inline bool
open(HtsFile & target, const char * f)
{
    target.filename = f;
    return target.open();
}

/**
 * @brief Opens a HTS from stream (e.g. std::cin).
 * 
 * @param target An empty HtsFile object.
 * @param s The stream to read from.
 */
inline bool
open(HtsFile & target, std::istream &)
{
    return open(target, "-");
}


/**
 * @brief Copies a header from a source HTS file and replaces the header of the target.
 * 
 * @param target Target HTS file.
 * @param source Source HTS file.
 */
inline void
copyHeader(HtsFile & target, HtsFile const & source)
{
    target.hdr = bam_hdr_dup(source.hdr);
}

/**
 * @brief Copies a record from a source HTS file and replaces the record of the target.
 * 
 * @param target Target HTS file.
 * @param source Source HTS file.
 */
inline void
copyRecord(HtsFile & target, HtsFile const & source)
{
    target.hts_record = bam_dup1(source.hts_record);
}

/**
 * @brief Loads an index for a HTS file using the default filename.
 * 
 * @param file HTS file to load index for.
 * @returns True on success, otherwise false.
 */
inline bool
loadIndex(HtsFile & file)
{
    file.hts_index = sam_index_load(file.fp, file.filename);
    return file.hts_index != nullptr;
}

/**
 * @brief Loads an index for a HTS file with a specific filename.
 * 
 * @param file HTS file to load index for.
 * @param indexFileName The filename of the index.
 */
inline bool
loadIndex(HtsFile & file, const char * indexFileName)
{
    file.hts_index = sam_index_load2(file.fp, file.filename, indexFileName);
    return file.hts_index != nullptr;
}

/**
 * @brief Builds an index for BAM or CRAM files using the default filename.
 * 
 * @param file The file to build index for.
 * @param min_shift Force a certain minimum amount of shift. (I think) smaller shifts mean more accurate queries at the cost of index size.
 *                  The default value is 0, which means the default value of htslib will be used.
 * @returns True on success, otherwise false.
 */
inline bool
buildIndex(HtsFile & file, int min_shift = 0)
{
    return !sam_index_build(file.filename, min_shift);
}

/**
 * @brief Builds an index for BAM or CRAM files using a specific filename.
 * 
 * @param file The file to build index for.
 * @param min_shift Force a certain minimum amount of shift. (I think) smaller shifts mean more accurate queries at the cost of index size.
 *                  The default value is 0, which means the default value of htslib will be used.
 * @returns True on success, otherwise false.
 */
inline bool
buildIndex(HtsFile & file, const char * indexFileName, int min_shift = 0)
{
    return !sam_index_build2(file.filename, indexFileName, min_shift);
}

/**
 * @brief Uses the index to go to a certain region of the HTS file.
 * 
 * @param file HTS file to change index on.
 * @param region The region to go to. Should be on one of these formats: chrX, chrX:A, or chrX:A-B.
 */
inline bool
setRegion(HtsFile & file, const char * region)
{
    if (file.hts_iter != nullptr)
        sam_itr_destroy(file.hts_iter);

    file.hts_iter = sam_itr_querys(file.hts_index, file.hdr, region);
    return file.hts_iter;
}

inline bool
setRegion(HtsFile & file, const char * chromosome, int start, int end)
{
    if (file.hts_iter != nullptr)
        sam_itr_destroy(file.hts_iter);

    char region[50];
    sprintf(region, "%s:%d-%d", chromosome, start, end);
    file.hts_iter = sam_itr_querys(file.hts_index, file.hdr, region);
    return file.hts_iter != nullptr;
}

inline bool
setRegion(HtsFile & file, int32_t tid, int32_t start, int32_t end)
{
    if (file.hts_iter != nullptr)
        sam_itr_destroy(file.hts_iter);

    file.hts_iter = sam_itr_queryi(file.hts_index, tid, start, end);
    return file.hts_iter;
}

/**
 * @brief Check if the have read the last of record of an input file.
 * 
 * @param file An input file.
 * @return True if we have read the last record.
 */
inline bool
atEnd(HtsFileIn const & file)
{
    return file.at_end;
}

/**
 * @brief Read the next record from a HTS file.
 * 
 * @param file HTS file to read from.
 * @returns True on success, otherwise false.
 */
inline bool
readRecord(HtsFile & file)
{
    if (sam_read1(file.fp, file.hdr, file.hts_record) >= 0)
    {
        return true;
    }

    file.at_end = true;
    return false;
}

/**
 * @brief Read the next record from a HTS file and parse it to a sequence record.
 * 
 * @param record Sequencing record to write to.
 * @param file HTS file to read from.
 * @returns True on success, otherwise false.
 */
inline bool
readRecord(HtsSequenceRecord & record, HtsFile & file)
{
    if (readRecord(file))
    {
        record.parse(file.hts_record);
        return true;
    }
    else
    {
        // We've reached the end of the file, or an error occured.
        return false;
    }
}

/**
 * @brief Reads an alignment record.
 * 
 * @param record The record to insert data into.
 * @param file The file to read from.
 */
inline bool
readRecord(BamAlignmentRecord & record, HtsFile & file)
{
    if (readRecord(file))
    {
        parse(record, file.hts_record);
        return true;
    }
    else
    {
        // We've reached the end of the file, or an error occured.
        return false;
    }
}

/**
 * @brief Read the next record from a region and parse it to a sequence record.
 * 
 * @param record Sequencing record to write to.
 * @param file HTS file to read from.
 * @returns True on success, otherwise false.
 */
inline bool
readRegion(HtsSequenceRecord & record, HtsFile & file)
{
    // std::cout << "s" << (file.fp == nullptr) << (file.hts_iter == nullptr) << (file.hts_record == nullptr) << std::endl;

    if (sam_itr_next(file.fp, file.hts_iter, file.hts_record) >= 0)
    {
        record.parse(file.hts_record);
        return true;
    }
    else
    {
        // We've reached the end of the file, or an error occured.
        return false;
    }
}

/**
 * @brief Writes a HTS header to disk.
 * 
 * @param file HTS file to get the header from.
 * @returns True on success, otherwise false.
 */
inline bool
writeHeader(HtsFileOut & file)
{
    return !sam_hdr_write(file.fp, file.hdr);
}

/**
 * @brief Writes a HTS record to disk. Returns 
 * 
 * @param file HTS file to get the record from.
 * @returns True on success, otherwise false.
 */
inline bool
writeRecord(HtsFile & file)
{
    return !sam_write1(file.fp, file.hdr, file.hts_record);
}

/**
 * @brief Writes a record to file.
 * 
 * @param file The file to write to.
 * @param record The record to write.
 */
inline bool
writeRecord(HtsFile & file, BamAlignmentRecord const & record)
{
    parse(file.hts_record, file.hdr, record);
    return writeRecord(file);
}


} // namespace seqan

#endif  // SEQAN_HTS_IO_HTS_FILE_IN_H_
