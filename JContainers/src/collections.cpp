

#include <boost/serialization/serialization.hpp>
#include <boost/serialization/export.hpp>

#include <boost/serialization/vector.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/variant.hpp>

#include <boost/serialization/split_member.hpp>
#include <boost/serialization/split_free.hpp>

#include <boost/serialization/version.hpp>

#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>

#include <fstream>
#include <sstream>
#include <set>

#include "gtest.h"

#include "intrusive_ptr.hpp"
#include "intrusive_ptr_serialization.hpp"
#include "util/istring_serialization.h"

#include "object/object_base_serialization.h"

#include "collections.h"
#include "tes_context.h"
#include "form_handling.h"

#include "tes_context.hpp"

BOOST_CLASS_EXPORT_GUID(collections::array, "kJArray");
BOOST_CLASS_EXPORT_GUID(collections::map, "kJMap");
BOOST_CLASS_EXPORT_GUID(collections::form_map, "kJFormMap");
BOOST_CLASS_EXPORT_GUID(collections::integer_map, "kJIntegerMap");

BOOST_CLASS_VERSION(collections::item, 2)

BOOST_CLASS_IMPLEMENTATION(boost::blank, boost::serialization::primitive_type);

namespace collections {

    // 0.67 to 0.68:
    namespace conv_067_to_3 {

        struct old_blank {
            template<class Archive>
            void serialize(Archive & ar, const unsigned int version) {}
        };

        struct object_ref_old {
            object_base *px = nullptr;

            template<class Archive>
            void serialize(Archive & ar, const unsigned int version) {
                ar & px;
            }
        };

        struct item_converter : boost::static_visitor < > {
            item::variant& varNew;
        
            explicit item_converter(item::variant& var) : varNew(var) {}

            template<class T> void operator() (T& val) {
                varNew = std::move(val);
            }

            void operator()(old_blank& val) {}

            void operator()(object_ref_old& val) {
                varNew = internal_object_ref(val.px);
            }
        };

        template<class A> void do_conversion(A& ar, item::variant& varNew) {
            typedef boost::variant<old_blank, SInt32, Float32, FormId, object_ref_old, std::string> variant_old;
            variant_old varOld;
            ar >> varOld;

            item_converter converter(varNew);
            boost::apply_visitor(converter, varOld);
        }
    }
    
    namespace conv_066 {
        // deprecate in 0.67:
        enum ItemType : unsigned char {
            ItemTypeNone = 0,
            ItemTypeInt32 = 1,
            ItemTypeFloat32 = 2,
            ItemTypeCString = 3,
            ItemTypeObject = 4,
            ItemTypeForm = 5,
        };

        template<class Archive>
        void read(Archive & ar, item& itm) {
            ItemType type = ItemTypeNone;
            ar & type;

            switch (type) {
            case ItemTypeInt32: {
                SInt32 val = 0;
                ar & val;
                itm = val;
                break;
            }
            case ItemTypeFloat32: {
                Float32 val = 0;
                ar & val;
                itm = val;
                break;
            }
            case ItemTypeCString: {
                std::string string;
                ar & string;
                itm = string;
                break;
            }
            case ItemTypeObject: {
                object_base *object = nullptr;
                ar & object;
                itm.var() = internal_object_ref(object, false);
                break;
            }
            case ItemTypeForm: {
                UInt32 oldId = 0;
                ar & oldId;
                itm.var() = (FormId)oldId;
                break;
            }
            default:
                break;
            }
        }
    }

    template<class Archive>
    void item::load(Archive & ar, const unsigned int version)
    {
        switch (version)
        {
        default:
            BOOST_ASSERT(false);
            break;
        case 2:
            ar & _var;
            break;
        case 1: {
            conv_067_to_3::do_conversion(ar, _var);
            break;
        }
        case 0:
            conv_066::read(ar, *this);
            break;
        }

        if (auto fId = get<FormId>()) {
            *this = form_handling::resolve_handle(*fId);
        }
    }

    template<class Archive>
    void item::save(Archive & ar, const unsigned int version) const {
        ar & _var;
    }

    //////////////////////////////////////////////////////////////////////////

    template<class Archive>
    void array::serialize(Archive & ar, const unsigned int version) {
        ar & boost::serialization::base_object<object_base>(*this);
        ar & _array;
    }

    template<class Archive>
    void map::serialize(Archive & ar, const unsigned int version) {
        ar & boost::serialization::base_object<object_base>(*this);
        ar & cnt;
    }

    template<class Archive>
    void form_map::serialize(Archive & ar, const unsigned int version) {
        ar & boost::serialization::base_object<object_base>(*this);
        ar & cnt;
    }

    template<class Archive>
    void integer_map::serialize(Archive & ar, const unsigned int version) {
        ar & boost::serialization::base_object<object_base>(*this);
        ar & cnt;
    }

    //////////////////////////////////////////////////////////////////////////

    void form_map::u_onLoaded() {

        for (auto itr = cnt.begin(); itr != cnt.end(); ) {

            const FormId& oldKey = itr->first;
            FormId newKey = form_handling::resolve_handle(oldKey);

            if (oldKey == newKey) {
                ++itr; // fine
            }
            else if (newKey == FormZero) {
                itr = cnt.erase(itr);
            }
            else if (oldKey != newKey) { // and what if newKey will replace another oldKey???

                // There is one issue. Given two plugins
                // .... A ... B..
                // both plugins gets swapped
                // and two form Id's swapped too: 0xaa001 swapped with 0xbb001
                // the form-id from the A replaces the form-id from the B

                auto anotherOldKeyItr = cnt.find(newKey);
                if (anotherOldKeyItr != cnt.end()) { // exactly that rare case, newKey equals to some other oldKey
                    // do not insert as it will replace other oldKey, SWAP values instead
                    std::swap(anotherOldKeyItr->second, itr->second);
                    ++itr;
                }
                else {
                    cnt.insert(container_type::value_type(newKey, std::move(itr->second)));
                    itr = cnt.erase(itr);
                }
            }
        }
    }

    //////////////////////////////////////////////////////////////////////////

    void array::u_nullifyObjects() {
        for (auto& item : _array) {
            item.u_nullifyObject();
        }
    }

    //////////////////////////////////////////////////////////////////////////
}