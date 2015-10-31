


                                     Zekvok 
                A directory backup utility for Windows desktops



                                      NAME

Zekvok: from "zek" and "vok"; correspondingly, the Dovahzul words for "back" and
"up".



                                  DESIGN GOALS

The primary design goals are, in no particular order:

* Transparency
* Security
* Efficiency

Backup TRANSPARENCY refers to the property that a backup should be, as much as
possible and/or practical, capable to be restored into an exact duplicate of the
data that was originally backup up.

Backup SECURITY is essential if the data being backed up is sensitive,
particularly if it's desirable to upload the backup to a cloud storage service.
Security covers both the ability to secure a backup from unauthorized readers,
and to detect malicious or accidental modifications. A properly secure backup
can be safely handed to an untrusted party for safekeeping.

Backup EFFICIENCY, while not as critical as the former two, is necessary if
frequent backups are desirable, doubly so if the dataset is large, octuply so if
the backup will be transferred over the Net. A smart system capable of trimming
away redundancies makes highly redundant backups feasible.



                                    FEATURES

Currently, all file system object types supported by both NTFS and Windows Vista
to 10 are supported, including:

* Regular files
* Regular directories (folders)
* File symbolic links
* Directory symbolic links
* Directory reparse points (junctions)
* Hardlinks

Note: NTFS also appears to support file reparse points, but as of this writing
no version of Windows supports this type of file system object.

Link-like file system objects are backuped up as links, storing their link
targets in the backup. Links to files don't create duplicate data, and links to
directories are not followed, nor are their targets traversed.
In the future, it will be possible to automatically add link targets as backup
sources, if they're not otherwise covered by any other backup source.

There are no known limits on path length or directory nesting. Paths as long as
2300 characters long and as deep as 50 levels deep have been successfully
tested.

Currently, the following NTFS features are neither backed up nor restored:

* Alternate data streams
* Access control lists
* File attributes (the archive flag is queried to optimize backup creation)
* File times (modification time is backup up to optimize backup creation)

                                  Backup modes

The following backup functionality is supported:

* Full backups
* Incremental backups (At file level. If a file is detected to have changed, a
  complete copy of the new version is backed up.)
* Whole-version compression (LZMA)
* Public-key-based backup encryption
* Backing up files in use by other programs (Volume Shadow Service required)
* Transacted backup creation (The backup storage will never be left in an
  inconsistent state, even if the backup process is interrupted by a power
  failure.)



                                PLANNED FEATURES

* More user-friendliness
* Block-level incremental backups (If a file is changed, only the portions that
  actually changed will be backed up.)
* Block- or file-level deduplication and move/rename detection

