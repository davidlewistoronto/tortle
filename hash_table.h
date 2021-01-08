#ifndef __HASH_TABLE_H
#define __HASH_TABLE_H

#include "utils.h"

#define hash_table_start_size	509

template<class T>
class hash_table_item {
public:
	unsigned name_hash_raw;
	char *name;
	T item;
};

template<class T>
class hash_table_list {
public:
	hash_table_list<T> *next;
	hash_table_item<T> name_val_pair;
};

template<class T> 
class hash_table {
protected:
	hash_table_list<T> **entries;
	int n_entries;
	int table_size;
	T null_ret_val;



public:
	void *operator new (size_t n);
	void operator delete (void *me);

	hash_table ();
	~hash_table ();

	void add (char *name, T val);
	T lookup (char *name);
	T& lookup_val (char *name);
	T* lookup_ptr (char *name);
	bool contains (char * name);
	hash_table_item<T> *lookup_pair (char *name);
};


template<class T> void *hash_table<T>::operator new (size_t n)
{	return smalloc (n);
}

template<class T> void hash_table<T>::operator delete (void *me)
{	smfree (me);
}

template<class T> hash_table<T>::hash_table () {
	int i;
	
	n_entries = 0;
	table_size = hash_table_start_size;
	entries = (hash_table_list<T> **) smalloc (sizeof (hash_table_list<T>*) * table_size);
	for (i = 0; i < table_size; i++) {
		entries [i] = (hash_table_list<T> *) NULL;
	}
	null_ret_val = (T) NULL;
}

template<class T> hash_table<T>::~hash_table () {
}

template<class T> bool hash_table<T>::contains (char *name) {
	int ibin;
	hash_table_list<T> *p;

	ibin = hash (name, table_size);
	for (p = entries [ibin]; p != NULL && strcmp (p->name_val_pair.name, name) != 0; p = p->next)
		;
	if (p == NULL)
		return false;
	else
		return true;
}

template<class T> T hash_table<T>::lookup (char *name) {
	int ibin;
	hash_table_list<T> *p;

	ibin = hash (name, table_size);
	for (p = entries [ibin]; p != NULL && strcmp (p->name_val_pair.name, name) != 0; p = p->next)
		;
	if (p == NULL)
		return null_ret_val;
	else
		return p->name_val_pair.item;
}

template<class T> T& hash_table<T>::lookup_val (char *name) {
	int ibin;
	hash_table_list<T> *p;

	ibin = hash (name, table_size);
	for (p = entries [ibin]; p != NULL && strcmp (p->name_val_pair.name, name) != 0; p = p->next)
		;
	if (p == NULL)
		return null_ret_val;
	else
		return p->name_val_pair.item;
}

template<class T> T* hash_table<T>::lookup_ptr (char *name) {
	int ibin;
	hash_table_list<T> *p;

	ibin = hash (name, table_size);
	for (p = entries [ibin]; p != NULL && strcmp (p->name_val_pair.name, name) != 0; p = p->next)
		;
	if (p == NULL)
		return NULL;
	else
		return &(p->name_val_pair.item);
}

template<class T> hash_table_item<T> * hash_table<T>::lookup_pair (char *name) {
	int ibin;
	hash_table_list<T> *p;

	ibin = hash (name, table_size);
	for (p = entries [ibin]; p != NULL && strcmp (p->name_val_pair.name, name) != 0; p = p->next)
		;
	if (p == NULL)
		return NULL;
	else
		return &(p->name_val_pair);
}
template<class T> void hash_table<T>::add (char *name, T val) {
	int ibin;
	int ibin_new;
	hash_table_list<T> *p;
	hash_table_list<T> **new_entries;
	int new_table_size;
	unsigned ubin;

	if (n_entries == table_size) {
		new_table_size = table_size * 517 / 253 + 17;
		new_entries = (hash_table_list<T> **) smalloc (sizeof (hash_table_list<T>*) * new_table_size);
		for (ibin_new = 0; ibin_new < new_table_size; ibin_new++) {
			new_entries [ibin_new] = (hash_table_list<T> *)NULL;
		}
		for (ibin = 0; ibin < table_size; ibin++) {
			while (entries [ibin] != (hash_table_list<T> *) NULL) {
				p = entries [ibin];
				entries [ibin] = p->next;
				ibin_new = p->name_val_pair.name_hash_raw % new_table_size;
				p->next = new_entries [ibin_new];
				new_entries [ibin_new] = p;
			}
		}
		smfree (entries);
		entries = new_entries;
		table_size = new_table_size;
	}
	ubin = uhash (name);
	ibin = (int) ubin % table_size;
	p = (hash_table_list<T> *) smalloc (sizeof (hash_table_list<T>));
	p->name_val_pair.name = newstring (name);
	p->name_val_pair.name_hash_raw = ubin;
	p->name_val_pair.item = val;
	p->next = entries [ibin];
	entries [ibin] = p;
	n_entries++;
}


#endif




 
