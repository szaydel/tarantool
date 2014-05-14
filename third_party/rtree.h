/*
 * Guttman's R-Tree 
 * Copyright (C) 2014 Mail.RU
 */

#ifndef __RTREE_H__
#define __RTREE_H__

#include <stdlib.h>

#define MAX_HEIGHT 16
#define DIMENSIONS 2

typedef int   coord_t;
typedef long  area_t;
typedef void* record_t;

#define AREA_MAX (~((area_t)1 << (sizeof(area_t)*8-1)))
#define AREA_MIN ((area_t)1 << (sizeof(area_t)*8-1))

#define RTREE_PAGE_SIZE 1024 /* R-Tree use linear search within element on the page, so larger page cause worse performance */

class R_tree;
class R_page;
class R_tree_iterator;

class rectangle_t
{
  public:
    enum { dim = 2 };
    coord_t boundary[dim*2];

    // Squarer of distance
    area_t distance2(coord_t const* point) const
    { 
        area_t d = 0;
        for (int i = 0; i < dim; i++) {             
            if (point[i] < boundary[i]) { 
                d += (boundary[i] - point[i]) * (boundary[i] - point[i]);
            } else if (point[i] > boundary[dim + i]) { 
                d += (boundary[dim + i] - point[i]) * (boundary[dim + i] - point[i]);
            }
        }
        return d;
    }
    

    friend area_t area(rectangle_t const& r) { 
        area_t area = 1;
        for (int i = dim; --i >= 0; area *= r.boundary[i+dim] - r.boundary[i]);
        return area;
    }

    void operator +=(rectangle_t const& r) { 
        int i = dim; 
        while (--i >= 0) { 
            boundary[i] = (boundary[i] <= r.boundary[i]) 
                ? boundary[i] : r.boundary[i];
            boundary[i+dim] = (boundary[i+dim] >= r.boundary[i+dim]) 
                ? boundary[i+dim] : r.boundary[i+dim];
        }
    }
    rectangle_t operator + (rectangle_t const& r) const { 
        rectangle_t res;
        int i = dim; 
        while (--i >= 0) { 
            res.boundary[i] = (boundary[i] <= r.boundary[i]) 
                ? boundary[i] : r.boundary[i];
            res.boundary[i+dim] = (boundary[i+dim] >= r.boundary[i+dim]) 
                ? boundary[i+dim] : r.boundary[i+dim];
        }
        return res;
    }
    bool operator& (rectangle_t const& r) const {
        int i = dim; 
        while (--i >= 0) { 
            if (boundary[i] > r.boundary[i+dim] ||
                r.boundary[i] > boundary[i+dim])
            {
                return false;
            }
        }
        return true;
    }
    bool operator <= (rectangle_t const& r) const { 
        int i = dim; 
        while (--i >= 0) { 
            if (boundary[i] < r.boundary[i] ||
                boundary[i+dim] > r.boundary[i+dim])
            {
                return false;
            }
        }
        return true;
    }
    bool operator < (rectangle_t const& r) const { 
        return *this <= r && *this != r;
    }

    bool operator >= (rectangle_t const& r) const { 
        return r <= *this;
    }
    bool operator > (rectangle_t const& r) const { 
        return r <= *this && *this != r;
    }

    bool operator == (rectangle_t const& r) const { 
        int i = dim*2; 
        while (--i >= 0) { 
            if (boundary[i] != r.boundary[i]) { 
                return false;
            }
        }
        return true;
    }
    bool operator != (rectangle_t const& r) const { 
        return !(*this == r);
    }
    bool operator_true(rectangle_t const&) const { 
        return true;
    }
};

enum Spatial_search_op
{
    SOP_ALL,
    SOP_EQUALS,
    SOP_CONTAINS,
    SOP_STRICT_CONTAINS,
    SOP_OVERLAPS,
    SOP_BELONGS,
    SOP_STRICT_BELONGS,
    SOP_NEIGHBOR
};

class R_tree_iterator
{
    friend class R_tree;
    struct { 
        R_page* page;
        int     pos;
    } stack[MAX_HEIGHT];
    
    struct Neighbor { 
        void*     child;
        Neighbor* next;
        int       level;
        area_t    distance;
    };

    typedef bool (rectangle_t::*comparator_t)(rectangle_t const& r) const;

    rectangle_t r;
    Spatial_search_op op;
    R_tree const* tree;
    Neighbor* list;
    Neighbor* free;
    bool eof;

    comparator_t intr_cmp;
    comparator_t leaf_cmp;
    
    bool goto_first(int sp, R_page* pg);
    bool goto_next(int sp);
    bool init(R_tree const* tree, rectangle_t const& r, Spatial_search_op op);
    void insert(Neighbor* node);

    Neighbor* new_neighbor(void* child, area_t distance, int level);
    void free_neighbor(Neighbor* n);        
  public:
    void reset();
    record_t next();

    R_tree_iterator();
    ~R_tree_iterator();
};
    
class R_page { 
  public:
    struct branch { 
        rectangle_t r;
        R_page*     p;
    };
    
    enum { 
        card = (RTREE_PAGE_SIZE-4)/sizeof(branch), // maximal number of branches at page
        min_fill = card/2        // minimal number of branches at non-root page
    };

    struct reinsert_list { 
        R_page* chain;
        int     level;
        reinsert_list() { chain = NULL; }
    };

    R_page* insert(rectangle_t const& r, record_t obj, int level);

    bool remove(rectangle_t const& r, record_t obj, int level, reinsert_list& rlist);

    rectangle_t cover() const;

    R_page* split_page(branch const& br);

    R_page* add_branch(branch const& br) { 
        if (n < card) { 
            b[n++] = br;
            return NULL;
        } else { 
            return split_page(br);
        }
    }
    void remove_branch(int i);

    void purge(int level);

    R_page* next_reinsert_page() const { return (R_page*)b[card-1].p; }

    R_page(rectangle_t const& rect, record_t obj);
    R_page(R_page* old_root, R_page* new_page);

    int    n; // number of branches at page
    branch b[card];
};

class R_tree 
{ 
    friend class R_tree_iterator;
  public: 
    unsigned size() const { 
        return n_records;
    }
    bool search(rectangle_t const& r, Spatial_search_op op, R_tree_iterator& iterator) const;    
    void insert(rectangle_t const& r, record_t obj);
    bool remove(rectangle_t const& r, record_t obj);
    void purge();
    R_tree();
    ~R_tree() { purge(); }

  protected:
    unsigned n_records;
    unsigned height;
    R_page*  root;
};

#endif



