#ifndef STRUCT_H
#define STRUCT_H

#include <vector>
#include <stdexcept>

struct Pot
{
  int VAL;
  int IN1;
  int IN2;
  int PWM;

  Pot(int val = 0, int in1 = 0, int in2 = 0, int pwm = 0) : VAL(val), IN1(in1), IN2(in2), PWM(pwm) {}
};

struct Column {
  Pot pot;
  int ledPin;
  
  Column(Pot p = Pot(), int l = 0) : pot(p), ledPin(l) {}
};

struct ColumnsStruct {
  std::vector<Column> columnsStruct;

  // Constructor that accepts an initializer list of Column objects
  ColumnsStruct(std::initializer_list<Column> cols) {
      columnsStruct = cols;
  }

  void setColumn(int index, const Column& column) {
    if (index >= 0 && index < columnsStruct.size()) {
      columnsStruct[index] = column;
    }
  }

  Column getColumn(int index) {
    if (index >= 0 && index < columnsStruct.size()) {
      return columnsStruct[index];
    }
    throw std::out_of_range("Index out of range");
  }

  Pot getPot(int index) {
    if (index >= 0 && index < columnsStruct.size()) {
      return columnsStruct[index].pot;
    }
    throw std::out_of_range("Index out of range");
  }

  int getLedPin(int index) {
    if (index >= 0 && index < columnsStruct.size()) {
      return columnsStruct[index].ledPin;
    }
    throw std::out_of_range("Index out of range");
  }

  int size() const {
    return columnsStruct.size();
  }
};



#endif 