/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *
 * @format
 */

import * as React from 'react';
import {useState} from 'react';
import {Button, StyleSheet, Text, TextInput, View} from 'react-native';
import type {ModelInfo} from 'react-native-pytorch-core';
import useNLPQAModelInference from '../useNLPQAModelInference';

const modelInfo: ModelInfo = {
  name: 'Bert Q&A',
  model: require('../../models/bert_qa.ptl'),
};

export default function QuestionAnsweringDemo() {
  const [text, setText] = useState('');
  const [question, setQuestion] = useState('');
  const {answer, processQA} = useNLPQAModelInference(modelInfo);

  function handleAsk() {
    processQA(text, question);
  }

  return (
    <View style={styles.container}>
      <TextInput
        style={[styles.item, styles.input]}
        placeholder="Text"
        placeholderTextColor="#CCC"
        multiline={true}
        value={text}
        onChangeText={setText}
      />
      <TextInput
        style={[styles.item, styles.input]}
        placeholder="Question"
        placeholderTextColor="#CCC"
        value={question}
        onChangeText={setQuestion}
      />
      <Button title="Ask" onPress={handleAsk} />
      <Text style={styles.item}>{answer}</Text>
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    padding: 10,
  },
  item: {
    margin: 10,
  },
  input: {
    borderWidth: 1,
    color: '#000',
  },
});