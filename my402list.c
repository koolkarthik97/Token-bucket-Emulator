
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>

#include "cs402.h"

#include "my402list.h"

int  My402ListLength(My402List* list){
	return list->num_members;
}
int  My402ListEmpty(My402List* list){
	if(list->num_members <=0)
		return 1;
	return 0;
}

int  My402ListAppend(My402List* list, void* obj){
	My402ListElem* element=(My402ListElem*)malloc(sizeof(My402ListElem));
	element->next=NULL;
	element->prev=NULL;
	element->obj=obj;
	if(My402ListEmpty(list)){
		list->anchor.next=element;
		list->anchor.prev=element;
		element->next=&(list->anchor);
		element->prev=&(list->anchor);
		list->num_members=list->num_members + 1;
		return 1;
	}else{
		My402ListElem* last=My402ListLast(list);
		last->next=element;
		element->prev=last;
		list->anchor.prev=element;
		element->next=&(list->anchor);
		list->num_members=list->num_members + 1;	
		return 1;
	}
	return 0;
}
int  My402ListPrepend(My402List* list, void* obj){
	My402ListElem* element=(My402ListElem*)malloc(sizeof(My402ListElem));;
	element->next=NULL;
	element->prev=NULL;
	element->obj=obj;
	if(My402ListEmpty(list)){
		list->anchor.next=element;
		list->anchor.prev=element;
		element->next=&(list->anchor);
		element->prev=&(list->anchor);
		list->num_members=list->num_members + 1;
		return 1;
	}else{
		My402ListElem* first=My402ListFirst(list);
		list->anchor.next=element;
		element->prev=&(list->anchor);
		element->next=first;
		first->prev=element;
		list->num_members=list->num_members + 1;
		return 1;
	}
	return 0;
}
void My402ListUnlink(My402List* list, My402ListElem* element){
	if(My402ListEmpty(list)==1)
		return;
	element->prev->next=element->next;
	element->next->prev=element->prev;
	list->num_members=list->num_members-1;
	
}
void My402ListUnlinkAll(My402List* list){
	list->anchor.next=&(list->anchor);
	list->anchor.prev=&(list->anchor);
	list->num_members=0;
}
int  My402ListInsertAfter(My402List* list, void* obj, My402ListElem* element){
	if(element==NULL)
		return My402ListAppend(list,obj);
	else{
		My402ListElem* newElem=(My402ListElem*)malloc(sizeof(My402ListElem));
		newElem->obj=obj;
		newElem->prev=element;
		newElem->next=element->next;
		element->next=newElem;
		newElem->next->prev=newElem;
		list->num_members=list->num_members + 1;
		return 1;	
	}

	return 0;
}
int  My402ListInsertBefore(My402List* list, void* obj, My402ListElem* element){
	if(element==NULL)
		return My402ListPrepend(list,obj);
	else{
		My402ListElem* newElem=(My402ListElem*)malloc(sizeof(My402ListElem));
		newElem->obj=obj;
		newElem->prev=element->prev;
		newElem->next=element;
		newElem->prev->next=newElem;
		element->prev=newElem;
		list->num_members=list->num_members + 1;
		return 1;
	}
	return 0;
}


My402ListElem *My402ListFirst(My402List* list){
	if(My402ListEmpty(list))
		return NULL;
	else
		return list->anchor.next;
}
My402ListElem *My402ListLast(My402List* list){
	if(My402ListEmpty(list))
		return NULL;
	else
		return list->anchor.prev;
}
My402ListElem *My402ListNext(My402List* list, My402ListElem* element){
	if(My402ListLast(list)==element)
		return NULL;
	else
		return element->next;
}
My402ListElem *My402ListPrev(My402List* list, My402ListElem* element){
	if(My402ListFirst(list)==element)
		return NULL;
	else
		return element->prev;
}

My402ListElem *My402ListFind(My402List* list, void* obj){
	if(My402ListEmpty(list)==1)
		return NULL;
	My402ListElem* curr=My402ListFirst(list);
	My402ListElem* last=curr->prev;//anchor node
	while(curr!=last){
		if(curr->obj==obj)
			return curr;
		else
			curr=curr->next;	
	}
	return NULL;
}

int My402ListInit(My402List* list){
	if(list==NULL)
		return 0;
	list->anchor.prev=&(list->anchor);
	list->anchor.next=&(list->anchor);
	list->anchor.obj=NULL;
	list->num_members=0;
	return 1;
}
