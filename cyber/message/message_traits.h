/******************************************************************************
 * Copyright 2018 The Apollo Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

#ifndef CYBER_MESSAGE_MESSAGE_TRAITS_H_
#define CYBER_MESSAGE_MESSAGE_TRAITS_H_

#include <string>

#include "cyber/base/macros.h"
#include "cyber/common/log.h"
#include "cyber/message/message_header.h"
#include "cyber/message/protobuf_traits.h"
#include "cyber/message/py_message_traits.h"
#include "cyber/message/raw_message_traits.h"

namespace apollo {
namespace cyber {
namespace message {

DEFINE_TYPE_TRAIT(HasByteSize, ByteSizeLong)
DEFINE_TYPE_TRAIT(HasType, TypeName)
DEFINE_TYPE_TRAIT(HasSetType, SetTypeName)
DEFINE_TYPE_TRAIT(HasGetDescriptorString, GetDescriptorString)
DEFINE_TYPE_TRAIT(HasDescriptor, descriptor)
DEFINE_TYPE_TRAIT(HasFullName, full_name)
DEFINE_TYPE_TRAIT(HasSerializeToString, SerializeToString)
DEFINE_TYPE_TRAIT(HasParseFromString, ParseFromString)
DEFINE_TYPE_TRAIT(HasSerializeToArray, SerializeToArray)
DEFINE_TYPE_TRAIT(HasParseFromArray, ParseFromArray)

template <typename T>
class HasSerializer {
 public:
  static constexpr bool value =
      HasSerializeToString<T>::value && HasParseFromString<T>::value &&
      HasSerializeToArray<T>::value && HasParseFromArray<T>::value;
};

// avoid potential ODR violation
template <typename T>
constexpr bool HasSerializer<T>::value;

/*
mark: cyber收发消息种类总分为三种：
a.由.proto文件编译生成的，这种消息都是继承自google::protobuf::Message
b.原始消息RawMessage，有文件raw_message实现，为了统一规范，该类实现了类似protobuf实现的接口函数
c.用于python通信的PyMessageWrap，由py_message文件实现，原理类似RawMessage。
为了给他们提供统一的对外接口，才有了该文件，对模板函数进行多次重载，用于实现不同的消息种类，最后给外面呈现的统一接口名字。
*/

// MessageType 有七个重载函数，根据四个条件不同情况进行不同6个的重载
//    1.有无接口TypeName
//    2.接口是静态接口还是成员接口
//    3.是否继承自 google::protobuf::Message
//    4.有无函数参数

//重载1：有TypeName接口，是成员函数，有函数参数
template <typename T,
          typename std::enable_if<HasType<T>::value &&
                                      std::is_member_function_pointer<
                                          decltype(&T::TypeName)>::value,
                                  bool>::type = 0>
std::string MessageType(const T& message) {
  return message.TypeName();
}

//重载2：有TypeName接口，不是成员函数（是静态接口），有函数参数
template <typename T,
          typename std::enable_if<HasType<T>::value &&
                                      !std::is_member_function_pointer<
                                          decltype(&T::TypeName)>::value,
                                  bool>::type = 0>
std::string MessageType(const T& message) {
  return T::TypeName();
}

//重载3：无TypeName接口，不继承自google::protobuf::Message，有函数参数
template <typename T,
          typename std::enable_if<
              !HasType<T>::value &&
                  !std::is_base_of<google::protobuf::Message, T>::value,
              bool>::type = 0>
std::string MessageType(const T& message) {
  return typeid(T).name();
}

//重载4：有TypeName接口，不是成员函数，没函数参数
template <typename T,
          typename std::enable_if<HasType<T>::value &&
                                      !std::is_member_function_pointer<
                                          decltype(&T::TypeName)>::value,
                                  bool>::type = 0>
std::string MessageType() {
  return T::TypeName();
}

//重载5：无TypeName接口，不继承自google::protobuf::Message，没函数参数
template <typename T,
          typename std::enable_if<
              !HasType<T>::value &&
                  !std::is_base_of<google::protobuf::Message, T>::value,
              bool>::type = 0>
std::string MessageType() {
  return typeid(T).name();
}

//重载6：有TypeName接口，是成员函数（是静态接口）,不继承自google::protobuf::Message，没函数参数
template <
    typename T,
    typename std::enable_if<
        HasType<T>::value &&
            std::is_member_function_pointer<decltype(&T::TypeName)>::value &&
            !std::is_base_of<google::protobuf::Message, T>::value,
        bool>::type = 0>
std::string MessageType() {
  return typeid(T).name();
}

// SetTypeName 两个重载，模板参数T有SetTypeName接口就调用，没有就不调用
template <typename T>
typename std::enable_if<HasSetType<T>::value, void>::type SetTypeName(
    const std::string& type_name, T* message) {
  message->SetTypeName(type_name);
}

template <typename T>
typename std::enable_if<!HasSetType<T>::value, void>::type SetTypeName(
    const std::string& type_name, T* message) {}

// ByteSize
// 两个重载，模板参数T有ByteSizeLong接口就调用，没有就不调用，直接返回-1
template <typename T>
typename std::enable_if<HasByteSize<T>::value, int>::type ByteSize(
    const T& message) {
  return static_cast<int>(message.ByteSizeLong());
}

template <typename T>
typename std::enable_if<!HasByteSize<T>::value, int>::type ByteSize(
    const T& message) {
  (void)message;
  return -1;
}

// FullByteSize 代表消息头+消息体的大小
template <typename T>
int FullByteSize(const T& message) {
  int content_size = ByteSize(message);
  if (content_size < 0) {
    return content_size;
  }
  return content_size + static_cast<int>(sizeof(MessageHeader));
}

// ParseFromArray
// 两个重载，模板参数T有ParseFromArray接口就调用，没有就不调用，直接返回false
template <typename T>
typename std::enable_if<HasParseFromArray<T>::value, bool>::type ParseFromArray(
    const void* data, int size, T* message) {
  return message->ParseFromArray(data, size);
}

template <typename T>
typename std::enable_if<!HasParseFromArray<T>::value, bool>::type
ParseFromArray(const void* data, int size, T* message) {
  return false;
}

// ParseFromString
// 两个重载，模板参数T有ParseFromString接口就调用，没有就不调用，直接返回false
template <typename T>
typename std::enable_if<HasParseFromString<T>::value, bool>::type
ParseFromString(const std::string& str, T* message) {
  return message->ParseFromString(str);
}

template <typename T>
typename std::enable_if<!HasParseFromString<T>::value, bool>::type
ParseFromString(const std::string& str, T* message) {
  return false;
}

// ParseFromHC 两个重载，HC我理解为HeaderContent的缩写，为消息头和消息体
//模板参数T有HasParseFromArray接口就做一番处理，没有直接返回false
template <typename T>
typename std::enable_if<HasParseFromArray<T>::value, bool>::type ParseFromHC(
    const void* data, int size, T* message) {
  const auto header_size = sizeof(MessageHeader);
  RETURN_VAL_IF(size < (int)header_size, false);
  const MessageHeader* header = static_cast<const MessageHeader*>(data);
  RETURN_VAL_IF((size - header_size) < header->content_size(), false);
  SetTypeName(header->msg_type(), message);
  return message->ParseFromArray(
      static_cast<const void*>(static_cast<const char*>(data) + header_size),
      header->content_size());
}

template <typename T>
typename std::enable_if<!HasParseFromArray<T>::value, bool>::type ParseFromHC(
    const void* data, int size, T* message) {
  return false;
}

// SerializeToArray
// 两个重载，模板参数T有SerializeToArray接口就调用，没有就不调用，直接返回false
template <typename T>
typename std::enable_if<HasSerializeToArray<T>::value, bool>::type
SerializeToArray(const T& message, void* data, int size) {
  return message.SerializeToArray(data, size);
}

template <typename T>
typename std::enable_if<!HasSerializeToArray<T>::value, bool>::type
SerializeToArray(const T& message, void* data, int size) {
  return false;
}

// SerializeToString
// 两个重载，模板参数T有SerializeToString接口就调用，没有就不调用，直接返回false
template <typename T>
typename std::enable_if<HasSerializeToString<T>::value, bool>::type
SerializeToString(const T& message, std::string* str) {
  return message.SerializeToString(str);
}

template <typename T>
typename std::enable_if<!HasSerializeToString<T>::value, bool>::type
SerializeToString(const T& message, std::string* str) {
  return false;
}

// SerializeToHC
// 两个重载，模板参数T有SerializeToArray接口就调用，没有就不调用，直接返回false
template <typename T>
typename std::enable_if<HasSerializeToArray<T>::value, bool>::type
SerializeToHC(const T& message, void* data, int size) {
  int msg_size = ByteSize(message);
  if (msg_size < 0) {
    return false;
  }
  const std::string& type_name = MessageType(message);
  MessageHeader header;
  header.set_msg_type(type_name.data(), type_name.size());
  header.set_content_size(msg_size);
  if (sizeof(header) > static_cast<size_t>(size)) {
    return false;
  }
  char* ptr = reinterpret_cast<char*>(data);
  memcpy(ptr, static_cast<const void*>(&header), sizeof(header));
  ptr += sizeof(header);
  int left_size = size - static_cast<int>(sizeof(header));
  return SerializeToArray(message, reinterpret_cast<void*>(ptr), left_size);
}

// GetDescriptorString 三个重载
template <typename T>
typename std::enable_if<!HasSerializeToArray<T>::value, bool>::type
SerializeToHC(const T& message, void* data, int size) {
  return false;
}

template <typename T, typename std::enable_if<HasGetDescriptorString<T>::value,
                                              bool>::type = 0>
void GetDescriptorString(const std::string& type, std::string* desc_str) {
  T::GetDescriptorString(type, desc_str);
}

template <typename T,
          typename std::enable_if<
              !HasGetDescriptorString<T>::value &&
                  !std::is_base_of<google::protobuf::Message, T>::value,
              bool>::type = 0>
void GetDescriptorString(const std::string& type, std::string* desc_str) {}

template <typename MessageT,
          typename std::enable_if<
              !std::is_base_of<google::protobuf::Message, MessageT>::value,
              int>::type = 0>
void GetDescriptorString(const MessageT& message, std::string* desc_str) {}

// GetFullName 两个重载
template <
    typename T, typename Descriptor,
    typename std::enable_if<HasFullName<Descriptor>::value, bool>::type = 0>
std::string GetFullName() {
  return T::descriptor()->full_name();
}

template <
    typename T, typename Descriptor,
    typename std::enable_if<!HasFullName<Descriptor>::value, bool>::type = 0>
std::string GetFullName() {
  return typeid(T).name();
}

// GetMessageName 三个重载
template <typename T,
          typename std::enable_if<
              HasDescriptor<T>::value &&
                  !std::is_base_of<google::protobuf::Message, T>::value,
              bool>::type = 0>
std::string GetMessageName() {
  return GetFullName<T, decltype(*T::descriptor())>();
}

template <typename T,
          typename std::enable_if<
              HasDescriptor<T>::value &&
                  std::is_base_of<google::protobuf::Message, T>::value,
              bool>::type = 0>
std::string GetMessageName() {
  return T::descriptor()->full_name();
}

template <typename T,
          typename std::enable_if<!HasDescriptor<T>::value, bool>::type = 0>
std::string GetMessageName() {
  return typeid(T).name();
}

}  // namespace message
}  // namespace cyber
}  // namespace apollo

#endif  // CYBER_MESSAGE_MESSAGE_TRAITS_H_
