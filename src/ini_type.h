/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ini_type.h Types related to reading/writing '*.ini' files. */

#ifndef INI_TYPE_H
#define INI_TYPE_H

#include <functional>

#include "core/pointer.h"
#include "core/forward_list.h"

#include "fileio_type.h"

/** Base class for named entities (items, groups) in an ini file. */
struct IniName {
private:
	const ttd_unique_free_ptr<char> name; ///< the name of this item

public:
	IniName (const char *name, size_t len = 0);

	const char *get_name (void) const
	{
		return name.get();
	}

	bool is_name (const char *name) const
	{
		return strcmp (this->name.get(), name) == 0;
	}

	bool is_name (const char *name, size_t len) const
	{
		return strncmp (this->name.get(), name, len) == 0 && this->name.get()[len] == 0;
	}

	struct NamePred {
		const char *const name;
		const size_t len;

		NamePred (const char *name, size_t len)
			: name(name), len(len)
		{
		}

		bool operator() (const IniName *item) const
		{
			return item->is_name (this->name, this->len);
		}
	};
};


/** Base class for entity lists (items, groups) in an ini file. */
template <typename T>
struct IniList : ForwardList <T, true> {
	/** Clear this IniList. */
	void clear (void)
	{
		T *p = this->detach_all();
		while (p != NULL) {
			T *next = p->next;
			delete p;
			p = next;
		}
	}

	/** IniList destructor. */
	~IniList()
	{
		this->clear();
	}

	/**
	 * Find an entry by name.
	 * @param name the name to search for
	 * @return the item by that name, or NULL if none was found
	 */
	T *find (const char *name)
	{
		return this->find_pred (std::bind2nd (std::mem_fun (&T::IniName::is_name), name));
	}

	const T *find (const char *name) const
	{
		return const_cast<IniList*>(this)->find (name);
	}

	/**
	 * Find an entry by name.
	 * @param name the name to search for
	 * @param len use only this many chars of name
	 * @return the item by that name, or NULL if none was found
	 */
	T *find (const char *name, size_t len)
	{
		return this->find_pred (typename T::IniName::NamePred (name, len));
	}

	const T *find (const char *name, size_t len) const
	{
		return const_cast<IniList*>(this)->find (name, len);
	}

	/**
	 * Remove an entry by name.
	 * @param name the name to remove
	 */
	T *remove (const char *name)
	{
		return this->remove_pred (std::bind2nd (std::mem_fun (&T::IniName::is_name), name));
	}
};


/** Types of groups */
enum IniGroupType {
	IGT_VARIABLES = 0, ///< Values of the form "landscape = hilly".
	IGT_LIST      = 1, ///< A list of values, separated by \n and terminated by the next group block.
	IGT_SEQUENCE  = 2, ///< A list of uninterpreted lines, terminated by the next group block.
};

/** A single "line" in an ini file. */
struct IniItem : ForwardListLink<IniItem>, IniName {
	typedef ForwardList<IniItem>::iterator iterator;
	typedef ForwardList<IniItem>::const_iterator const_iterator;

	char *value;   ///< The value of this item
	char *comment; ///< The comment associated with this item

	IniItem (const char *name, size_t len = 0);
	~IniItem();

	void SetValue(const char *value);
};

/** A group within an ini file. */
struct IniGroup : ForwardListLink<IniGroup>, IniName, IniList<IniItem> {
	typedef ForwardList<IniGroup>::iterator iterator;
	typedef ForwardList<IniGroup>::const_iterator const_iterator;

	const IniGroupType type;   ///< type of group
	char *comment;       ///< comment for group

	IniGroup (IniGroupType type, const char *name, size_t len = 0);
	~IniGroup();

	IniItem *get_item (const char *name);

	IniItem *append (const char *name, size_t len = 0)
	{
		IniItem *item = new IniItem (name, len);
		this->IniList<IniItem>::append (item);
		return item;
	}
};

/** Ini file that only supports loading. */
struct IniLoadFile : IniList<IniGroup> {
	char *comment;                        ///< last comment in file
	const char * const *const list_group_names; ///< NULL terminated list with group names that are lists
	const char * const *const seq_group_names;  ///< NULL terminated list with group names that are sequences.

	IniLoadFile(const char * const *list_group_names = NULL, const char * const *seq_group_names = NULL);
	virtual ~IniLoadFile();

	IniGroupType get_group_type (const char *name, size_t len) const;

	IniGroup *get_group (const char *name, size_t len = 0);

	IniGroup *append (const char *name, size_t len)
	{
		IniGroupType type = this->get_group_type (name, len);
		IniGroup *group = new IniGroup (type, name, len);
		this->IniList<IniGroup>::append (group);
		return group;
	}

	void LoadFromDisk(const char *filename, Subdirectory subdir);

	/**
	 * Open the INI file.
	 * @param filename Name of the INI file.
	 * @param subdir The subdir to load the file from.
	 * @param size [out] Size of the opened file.
	 * @return File handle of the opened file, or \c NULL.
	 */
	virtual FILE *OpenFile(const char *filename, Subdirectory subdir, size_t *size) = 0;

	/**
	 * Report an error about the file contents.
	 * @param pre    Prefix text of the \a buffer part.
	 * @param buffer Part of the file with the error.
	 * @param post   Suffix text of the \a buffer part.
	 */
	virtual void ReportFileError(const char * const pre, const char * const buffer, const char * const post) = 0;
};

/** Ini file that supports both loading and saving. */
struct IniFile : IniLoadFile {
	IniFile (const char *filename, Subdirectory subdir, const char * const *list_group_names = NULL);

	bool SaveToDisk(const char *filename);

	virtual FILE *OpenFile(const char *filename, Subdirectory subdir, size_t *size);
	virtual void ReportFileError(const char * const pre, const char * const buffer, const char * const post);
};

#endif /* INI_TYPE_H */
