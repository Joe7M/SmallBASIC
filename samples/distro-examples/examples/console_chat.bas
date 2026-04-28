' ELIZA 5.0 - Enhanced Emotion + Markov + Memory-Aware + Context-Sensitive Chatbot
' Improved version with better context awareness, personality, and response generation
randomize timer
cls

' --- Configuration and initialization
debug_mode = 0  ' Set to 1 to enable debug output
max_memory = 15  ' Increased memory capacity
max_mood_log = 30  ' Longer mood tracking
start_time = timer()
session_length = 0

' --- Memory and mood tracking with enhanced structures
dim memory[max_memory], memory_emotion[max_memory], memory_importance[max_memory]
dim mood_log[max_mood_log], conversation_topics[20]
markov = {}
personality_traits = {"empathy": num_val(7), "curiosity": num_val(6), "patience": num_val(8)}
mem_index = 0 : mood_index = 0 : topic_index = 0
last_topic = "" : last_name$ = "" : repeat_count = 0
last_input$ = "" : questions_asked = 0 : statements_made = 0
last_response_type = "question"  ' Track if last response was question or statement

' --- Reflection map for pronouns
reflections = {
  "i":"you", "me":"you", "my":"your", "am":"are", "was":"were",
  "you":"me", "your":"my", "mine":"yours", "myself":"yourself",
  "yourself":"myself", "are":"am", "were":"was", "we":"you"
}

' --- Expanded emotion and topics vocabulary
positive_words = ["happy", "excited", "good", "love", "joy", "hope", "content", 
                 "grateful", "proud", "relieved", "peaceful", "enthusiastic", 
                 "optimistic", "glad", "pleased", "satisfied", "delighted"]

negative_words = ["sad", "angry", "upset", "hate", "tired", "depressed", "anxious", 
                 "lonely", "frustrated", "guilty", "ashamed", "worried", "afraid", 
                 "disappointed", "hurt", "confused", "overwhelmed", "stressed", 
                 "hopeless", "regretful", "insecure", "jealous"]

neutral_words = ["thinking", "wondering", "considering", "pondering", "reflecting",
                "curious", "interested", "unsure", "uncertain"]

intensity_modifiers = ["very", "extremely", "slightly", "somewhat", "incredibly", 
                      "mildly", "deeply", "profoundly", "a bit", "a little", "rather"]

sub_emotions = {
  "guilt":"guilty", "shame":"ashamed", "hope":"hopeful", "frustration":"frustrated", 
  "love":"loved", "anxiety":"anxious", "fear":"afraid", "confusion":"confused",
  "excitement":"excited", "pride":"proud", "gratitude":"grateful", "envy":"envious",
  "disgust":"disgusted", "happiness":"happy", "contentment":"content", "joy":"joyful",
  "appreciation":"appreciative", "worry":"worried", "disappointment":"disappointed"
}

' --- Expanded topics and improved categorization
topic_keywords = {
  "relationship": ["partner", "love", "husband", "wife", "boyfriend", "girlfriend", 
                  "dating", "marriage", "divorce", "breakup", "romance", "couple", 
                  "significant other", "ex", "spouse", "romantic"],
  "family": ["parents", "mother", "father", "mom", "dad", "brother", "sister", "sibling", 
            "child", "daughter", "son", "grandparent", "relative", "uncle", "aunt", "family"],
  "work": ["job", "boss", "career", "colleague", "work", "office", "promotion", 
          "fired", "workplace", "business", "professional", "company", "coworker", 
          "employment", "interview", "salary", "corporate"],
  "school": ["student", "teacher", "class", "school", "college", "university", 
            "exam", "course", "education", "study", "learning", "homework", 
            "professor", "grade", "degree", "academic"],
  "health": ["health", "sick", "doctor", "illness", "disease", "pain", "symptom", 
            "medication", "therapy", "hospital", "diet", "exercise", "wellness", 
            "fitness", "mental health", "physical", "diagnosis"],
  "self": ["lonely", "worthless", "sad", "angry", "tired", "depressed", "anxious", 
          "identity", "confidence", "self-esteem", "personal", "insecure", "myself", 
          "appearance", "personality", "character", "growth", "improvement"],
  "future": ["plan", "dream", "goal", "aspiration", "future", "hope", "ambition", 
            "intention", "objective", "desire", "opportunity", "potential", "prospect"],
  "past": ["memory", "regret", "mistake", "nostalgia", "reminisce", "childhood", 
          "history", "used to", "remember", "experience", "trauma", "forgiveness"]
}

' --- Enhanced response templates with more context sensitivity
responses = [
  "Tell me more about that.",
  "Why do you say that?",
  "That seems important to you.",
  "How long has this been going on?",
  "What do you think is at the root of that?",
  "That's interesting. Could you elaborate?",
  "Can you share more about your feelings around this?",
  "What makes you feel that way?",
  "How does that affect your daily life?",
  "What would make that situation better?",
  "Have you always felt this way about it?",
  "What thoughts come to mind when you consider this?",
  "Is there a pattern you've noticed here?"
]

deep_insights = [
  "Sometimes, the questions we avoid are the ones we most need to ask.",
  "It's okay to feel unsure - clarity often comes after reflection.",
  "Our thoughts are powerful, even when unspoken.",
  "Emotions are messengers, not enemies.",
  "Healing isn't linear, and that's okay.",
  "The stories we tell ourselves shape our reality.",
  "Growth happens at the edge of our comfort zone.",
  "Sometimes what we're seeking is already within us.",
  "The way we talk to ourselves matters deeply.",
  "Acknowledging our feelings is the first step to understanding them.",
  "Our past shapes us, but it doesn't have to define us.",
  "The most important relationship is the one we have with ourselves.",
  "Sometimes strength means allowing ourselves to be vulnerable.",
  "Perspective can transform our experience without changing our circumstances."
]

' --- Expanded vocabulary for emotional generation
vocab = ["i", "feel", "you", "are", "happy", "sad", "think", "want", "because", 
        "my", "your", "friend", "tired", "lonely", "hope", "love", "hate", "why", 
        "is", "do", "understand", "believe", "know", "see", "hear", "remember", 
        "dream", "wish", "need", "try", "help", "life", "time", "day", "good", 
        "bad", "fear", "courage", "mind", "heart", "soul", "peace", "struggle"]

' --- Enhanced bigram models for more natural text generation
bigrams_positive = {
  "i": ["feel", "am", "hope", "believe", "appreciate", "love", "enjoy", "understand"],
  "feel": ["happy", "content", "loved", "grateful", "peaceful", "excited", "hopeful"],
  "you": ["are", "help", "care", "understand", "deserve", "bring", "create"],
  "hope": ["you", "things", "life", "tomorrow", "someday", "possibilities", "experiences"],
  "life": ["is", "feels", "brings", "offers", "contains", "provides", "gives"],
  "love": ["you", "life", "this", "feeling", "sharing", "experiencing", "growing"],
  "my": ["heart", "life", "journey", "experience", "happiness", "growth", "peace"],
  "the": ["future", "beauty", "joy", "connection", "moment", "possibilities", "journey"]
}

bigrams_negative = {
  "i": ["feel", "am", "hate", "can't", "don't", "worry", "regret", "fear"],
  "feel": ["sad", "tired", "ashamed", "anxious", "scared", "overwhelmed", "broken"],
  "you": ["don't", "never", "hurt", "left", "ignored", "misunderstood", "abandoned"],
  "life": ["is", "has been", "hurts", "disappoints", "drains", "exhausts", "burdens"],
  "my": ["pain", "mistake", "fault", "failure", "regret", "anxiety", "sadness"],
  "hate": ["you", "this", "me", "myself", "everything", "feeling", "struggling"],
  "can't": ["handle", "understand", "bear", "face", "overcome", "escape", "accept"],
  "nobody": ["understands", "cares", "listens", "helps", "sees", "values", "respects"]
}

bigrams_neutral = {
  "i": ["wonder", "think", "ponder", "question", "consider", "contemplate", "observe"],
  "am": ["curious", "thinking", "wondering", "considering", "exploring", "analyzing"],
  "perhaps": ["we", "it", "there", "the", "some", "one", "that"],
  "what": ["if", "about", "would", "could", "might", "do", "does"],
  "sometimes": ["i", "we", "people", "things", "life", "moments", "thoughts"],
  "consider": ["this", "that", "how", "why", "when", "where", "who"],
  "maybe": ["you", "we", "it's", "there's", "the", "this", "that"]
}

' --- Question templates for more dynamic conversation
questions = [
  "How do you feel about %s?",
  "What's your experience with %s?",
  "When did you first notice %s?",
  "How has %s affected your life?",
  "What would change if %s were different?",
  "Have you spoken with anyone else about %s?",
  "What do you think caused %s?",
  "How do you cope with %s?",
  "What aspects of %s concern you most?",
  "If you could change one thing about %s, what would it be?",
  "How might someone else view %s?",
  "What emotions come up when you think about %s?"
]

' --- Personality-based response variations
personality_responses = {
  "empathetic": [
    "That sounds really difficult. How are you coping?",
    "I can hear how much that means to you.",
    "It's understandable to feel that way given what you've been through.",
    "That must have been painful for you.",
    "I'm here with you through these feelings."
  ],
  "curious": [
    "What fascinates you most about that?",
    "I wonder what would happen if you tried a different approach?",
    "That's an interesting perspective. How did you arrive at it?",
    "What aspects of this situation haven't you explored yet?",
    "I'm curious about what led you to that conclusion."
  ],
  "analytical": [
    "Let's break this down into parts we can examine.",
    "There seem to be several factors at play here.",
    "If we look at this objectively, what patterns emerge?",
    "What evidence supports this interpretation?",
    "It might help to analyze the cause and effect relationships here."
  ],
  "supportive": [
    "You've shown remarkable resilience.",
    "I believe in your ability to handle this.",
    "You've overcome challenges before, and you can do it again.",
    "Your feelings are valid and important.",
    "I'm here to support you through this process."
  ]
}

' --- Output functions with improved formatting
sub speak(txt)
  local i, c
  color 14
  print : print "eliza: ";
  for i = 1 to len(txt)
    c = mid(txt, i, 1)
    print c;
    delay 5 + rnd * 3
  next
  print : print
  color 15

  ' Track conversation statistics
  if right(txt, 1) = "?" then
    questions_asked += 1
    last_response_type = "question"
  else
    statements_made += 1
    last_response_type = "statement"
  end if
end sub

sub debug(txt)
  if debug_mode = 1 then
    color 8
    print "[DEBUG] " + txt
    color 15
  end if
end sub

func haskey(obj, k)
  return ismap(obj) && isarray(obj[k])
end func

' --- Enhanced reflection with improved contextual understanding
func reflect(txt)
  local result, w
  result = ""
  split(lcase(txt), " ", words)
  for w in words
    if haskey(reflections, w) then
      result += reflections[w] + " "
    else
      result += w + " "
    end if
  next
  return rtrim(result)
end func

' --- Detect emotion with intensity modifiers
func emotion(txt)
  local i, j, intensity = 1

  ' Check for intensity modifiers
  for i = 0 to ubound(intensity_modifiers)
    if instr(txt, intensity_modifiers[i]) then 
      intensity = 2
      exit for
    end if
  next

  for i = 0 to ubound(positive_words)
    if instr(txt, positive_words[i]) then return "positive" + str(intensity)
  next

  for i = 0 to ubound(negative_words)
    if instr(txt, negative_words[i]) then return "negative" + str(intensity)
  next

  for i = 0 to ubound(neutral_words)
    if instr(txt, neutral_words[i]) then return "neutral"
  next

  return "neutral"
end func

func emotion_to_base(emotion_str)
  if left(emotion_str, 8) = "positive" then return "positive"
  if left(emotion_str, 8) = "negative" then return "negative"
  return "neutral"
end func

func sub_emotion(txt)
  local k
  for k in sub_emotions
    if instr(txt, k) then return k
  next
  return ""
end func

' --- Improved topic detection with multiple topic awareness
func detect_topics(txt)
  local topic, keyw, i, found_topics = [], count = 0

  for topic in topic_keywords
    keyw = topic_keywords[topic]
    for i = 0 to ubound(keyw)
      if instr(txt, keyw[i]) then 
        debug("Topic detected: " + topic)
        found_topics[count] = topic
        count += 1
        exit for  ' Found one keyword for this topic
      end if
    next
  next

  return found_topics
end func

' --- Enhanced name detection with pronoun awareness
func detect_name(txt)
  split(txt, " ", words)

  ' Look for patterns like "my friend John" or "my mother Sarah"
  relationship_indicators = ["friend", "mother", "father", "brother", "sister", 
                           "partner", "boyfriend", "girlfriend", "colleague", "boss", 
                           "neighbor", "roommate", "cousin", "therapist", "doctor"]

  for i = 0 to ubound(words) - 1
    if words[i] = "my" then
      for j = 0 to ubound(relationship_indicators)
        if i + 1 <= ubound(words) and words[i+1] = relationship_indicators[j] then
          if i + 2 <= ubound(words) and len(words[i+2]) > 1 and left(words[i+2], 1) = ucase(left(words[i+2], 1)) then
            return words[i+2]  ' Likely a proper name
          end if
        end if
      next

      ' Check for direct "my Name" pattern if next word starts with capital
      if i + 1 <= ubound(words) and len(words[i+1]) > 1 and left(words[i+1], 1) = ucase(left(words[i+1], 1)) then
        ' Check if it's not just a sentence start
        if i > 0 then return words[i+1]
      end if
    end if
  next

  return ""
end func

' --- Improved mood analysis with weighted recent bias
func mood_summary()
  local pos, neg, recent_pos, recent_neg, i, weight
  pos = 0 : neg = 0 : recent_pos = 0 : recent_neg = 0

  for i = 0 to max_mood_log - 1
    if i < mood_index then  ' Only count valid entries
      ' Recent entries count more
      weight = 1
      if (mood_index - i < 5) or (mood_index < 5 and max_mood_log - i + mood_index < 5) then
        weight = 2  ' Recent entries weighted more heavily
      end if

      if left(mood_log[i], 8) = "positive" then 
        pos += weight
        if mid(mood_log[i], 9, 1) = "2" then pos += 1  ' Intensity modifier
        if (mood_index - i < 3) or (mood_index < 3 and max_mood_log - i + mood_index < 3) then
          recent_pos += 1
        end if
      end if

      if left(mood_log[i], 8) = "negative" then 
        neg += weight
        if mid(mood_log[i], 9, 1) = "2" then neg += 1  ' Intensity modifier
        if (mood_index - i < 3) or (mood_index < 3 and max_mood_log - i + mood_index < 3) then
          recent_neg += 1
        end if
      end if
    end if
  next

  debug("Mood analysis: pos=" + str(pos) + ", neg=" + str(neg) + ", recent_pos=" + str(recent_pos) + ", recent_neg=" + str(recent_neg))

  ' Check for shifts in mood
  if recent_pos > 2 and neg > pos then return "I notice you seem more positive in your recent messages."
  if recent_neg > 2 and pos > neg then return "I notice your mood seems to have shifted recently."

  ' Overall mood trends
  if pos > neg + 5 then return "Overall, you've been expressing more positive emotions in our conversation."
  if neg > pos + 5 then return "I've noticed you've been sharing a lot of difficult feelings during our talk."
  if pos > neg + 2 then return "There seems to be an undercurrent of hope in what you're sharing."
  if neg > pos + 2 then return "You seem to be carrying some emotional weight right now."

  return ""
end func

func current_mood()
  local pos, neg, i, recent_count = 0, weight
  pos = 0 : neg = 0 

  ' Only look at the 5 most recent entries for current mood
  for i = 0 to 4
    idx = (mood_index - 1 - i + max_mood_log) mod max_mood_log
    if mood_log[idx] <> "" then
      recent_count += 1
      weight = 5 - i  ' Most recent has highest weight

      if left(mood_log[idx], 8) = "positive" then 
        pos += weight
        if mid(mood_log[idx], 9, 1) = "2" then pos += weight \ 2  ' Intensity modifier
      end if

      if left(mood_log[idx], 8) = "negative" then 
        neg += weight
        if mid(mood_log[idx], 9, 1) = "2" then neg += weight \ 2  ' Intensity modifier
      end if
    end if
  next

  if recent_count < 2 then return "neutral"  ' Not enough data
  if pos > neg + 2 then return "positive"
  if neg > pos + 2 then return "negative"
  return "neutral"
end func

' --- Enhanced memory recall with better contextual relevance
func recall_memory(current_topic)
  local i, best, maxscore, score, topics
  maxscore = -1 : best = -1

  for i = 0 to max_memory - 1
    if memory[i] <> "" then
      score = 0

      ' Topic matching is a strong signal
      if current_topic <> "" then
        topics = detect_topics(memory[i])
        for t = 0 to ubound(topics)
          if topics[t] = current_topic then score += 5
        next
      end if

      ' Emotional content is relevant
      if memory_emotion[i] <> "neutral" then score += 2
      if left(memory_emotion[i], 8) = "negative" then score += 1
      if mid(memory_emotion[i], 9, 1) = "2" then score += 1  ' Intensity

      ' Specific emotional themes get priority
      if instr(memory[i], "guilt") or instr(memory[i], "shame") then score += 2
      if instr(memory[i], "hope") or instr(memory[i], "dream") then score += 1
      if instr(memory[i], "afraid") or instr(memory[i], "scared") then score += 2
      if instr(memory[i], "love") or instr(memory[i], "relationship") then score += 2

      ' Factor in manually assigned importance
      score += memory_importance[i]

      ' Recent memories are slightly preferred but not too strongly
      if (mem_index - i + max_memory) mod max_memory < max_memory \ 3 then score += 1

      if score > maxscore then
        maxscore = score
        best = i
      end if
    end if
  next

  if best >= 0 then return memory[best]
  return ""
end func

' --- Update and use Markov
sub update_markov(txt)
  local i, arr, j, words
  split(txt, " ", words)
  for i = 0 to ubound(words) - 1
    w1 = words[i]
    w2 = words[i+1]

    if (haskey(markov, w1)) then
      arr = markov[w1]
      j = len(arr) - 1
      arr[j] = w2
      markov[w1] = arr
    else
      markov[w1] = [w2]
    end if
  next
end sub

func generate_markov_sentence(seed$, max_length)
  local word$, i, sentence$, next_options, next_word$

  if seed$ = "" then
    ' Choose a random seed from available keys
    keys = []
    for k in markov
      j = len(keys)
      keys[j] = k
    next
    if len(keys) = 0 then return "I'm still learning from our conversation."
    seed$ = keys[int(rnd * len(keys))]
  end if

  word$ = seed$ : sentence$ = word$
  for i = 1 to max_length
    if haskey(markov, word$) then
      next_options = markov[word$]
      if len(next_options) = 0 then exit
      next_word$ = next_options[int(rnd * len(next_options))]
      sentence$ += " " + next_word$
      word$ = next_word$
    else
      exit
    end if
  next

  ' Ensure sentence ends properly
  if right(sentence$, 1) <> "." and right(sentence$, 1) <> "?" and right(sentence$, 1) <> "!" then
    sentence$ += "."
  end if

  ' Capitalize first letter
  if len(sentence$) > 0 then
    sentence$ = ucase(left(sentence$, 1)) + mid(sentence$, 2)
  end if

  return sentence$
end func

' --- Improved emotional sentence generator with better coherence
func predict_next_emotional(prev$, mood$)
  if mood$ = "positive" and haskey(bigrams_positive, prev$) then
    return bigrams_positive[prev$][int(rnd * len(bigrams_positive[prev$]))]
  elseif mood$ = "negative" and haskey(bigrams_negative, prev$) then
    return bigrams_negative[prev$][int(rnd * len(bigrams_negative[prev$]))]
  elseif mood$ = "neutral" and haskey(bigrams_neutral, prev$) then
    return bigrams_neutral[prev$][int(rnd * len(bigrams_neutral[prev$]))]
  end if
  return vocab[int(rnd * len(vocab))]
end func

func generate_sentence_emotional(seed$, mood$, max_length)
  local word$, i, sentence$

  if seed$ = "" then
    ' Choose appropriate seed based on mood
    if mood$ = "positive" then
      seed_options = ["i", "hope", "life", "love"]
      seed$ = seed_options[int(rnd * len(seed_options))]
    elseif mood$ = "negative" then
      seed_options = ["i", "feel", "sometimes", "life"]
      seed$ = seed_options[int(rnd * len(seed_options))]
    else
      seed_options = ["i", "perhaps", "sometimes", "maybe"]
      seed$ = seed_options[int(rnd * len(seed_options))]
    end if
  end if

  word$ = seed$ : sentence$ = word$
  for i = 1 to max_length
    word$ = predict_next_emotional(word$, mood$)
    sentence$ += " " + word$
  next

  ' Ensure sentence ends properly
  if right(sentence$, 1) <> "." and right(sentence$, 1) <> "?" and right(sentence$, 1) <> "!" then
    sentence$ += "."
  end if

  ' Capitalize first letter
  if len(sentence$) > 0 then
    sentence$ = ucase(left(sentence$, 1)) + mid(sentence$, 2)
  end if

  return sentence$
end func

' --- Adaptive response selection with more context awareness
func adaptive(txt, mood$, topics_arr)
  split(txt, " ", words)

  ' Adjust for message length and complexity
  if ubound(words) + 1 <= 3 then return "Can you say more about that?"
  if ubound(words) + 1 > 15 then return "That's quite a lot to process. What aspect feels most important right now?"

  ' Balance question/statement responses
  if last_response_type = "question" and rnd < 0.7 then
    ' More likely to make a statement after a question
    if len(topics_arr) > 0 and rnd < 0.6 then
      ' Topic-focused response
      topic_idx = int(rnd * len(topics_arr))
      if haskey(personality_responses, "analytical") then
        return personality_responses["analytical"][int(rnd * len(personality_responses["analytical"]))]
      end if
    elseif mood$ = "negative" and rnd < 0.7 then
      ' Emotional support for negative moods
      if haskey(personality_responses, "empathetic") then
        return personality_responses["empathetic"][int(rnd * len(personality_responses["empathetic"]))]
      end if
    elseif mood$ = "positive" and rnd < 0.6 then
      ' Encourage elaboration for positive moods
      if haskey(personality_responses, "supportive") then
        return personality_responses["supportive"][int(rnd * len(personality_responses["supportive"]))]
      end if
    end if
  else
    ' More likely to ask a question after a statement
    if len(topics_arr) > 0 and rnd < 0.7 then
      topic_idx = int(rnd * len(topics_arr))
      topic = topics_arr[topic_idx]
      question_idx = int(rnd * len(questions))
      return replace(questions[question_idx], "%s", topic)
    end if
  end if

  ' Default response
  return responses[int(rnd * len(responses))]
end func

func get_response(url$)
  local json$ = array(run("curl -s " + "\"" + url$ + "\""))
  if len(json$) > 0 then return json$[0].content
  return "I couldn't find an inspiring quote right now."
end func

' --- New function: Generate contextual questions based on topic
func generate_question(topic$)
  if topic$ = "" then
    general_questions = [
      "What's been on your mind lately?",
      "How have you been feeling recently?",
      "What would you like to explore today?",
      "Is there something specific you'd like to talk about?",
      "What matters to you right now?"
    ]
    return general_questions[int(rnd * len(general_questions))]
  end if

  topic_questions = {
    "relationship": [
      "How has this relationship evolved over time?",
      "What qualities do you value most in relationships?",
      "How do you handle conflicts in this relationship?",
      "What patterns have you noticed in your relationships?",
      "How does this relationship compare to your expectations?"
    ],
    "family": [
      "How have your family dynamics shaped you?",
      "What family traditions or values are important to you?",
      "How has your relationship with your family changed over time?",
      "What unspoken rules exist in your family?",
      "What role do you typically play in your family?"
    ],
    "work": [
      "What aspects of your work bring you satisfaction?",
      "How does your work align with your values?",
      "What would make your work situation ideal?",
      "How do you handle work-related stress?",
      "What skills would you like to develop professionally?"
    ],
    "school": [
      "What motivates your educational pursuits?",
      "How do you deal with academic pressure?",
      "What has been your most meaningful learning experience?",
      "How do you balance your educational goals with other aspects of life?",
      "What would make your learning experience more fulfilling?"
    ],
    "health": [
      "How do you prioritize self-care in your routine?",
      "What does wellness mean to you personally?",
      "How do you balance physical and mental health needs?",
      "What healthy habits would you like to develop?",
      "How does your health impact other areas of your life?"
    ],
    "self": [
      "What aspects of yourself are you trying to understand better?",
      "How would you describe your relationship with yourself?",
      "What personal growth have you noticed recently?",
      "Which of your qualities do you value most?",
      "What inner conflicts are you working through?"
    ],
    "future": [
      "How do you envision your ideal future?",
      "What steps are you taking toward your goals?",
      "What fears do you have about the future?",
      "What possibilities excite you most?",
      "How do you balance planning with being present?"
    ],
    "past": [
      "How has your past shaped your current perspective?",
      "What experiences have been most transformative for you?",
      "How do you make peace with difficult memories?",
      "What lessons have you carried forward from past experiences?",
      "How accurately do you think you remember your past?"
    ]
  }

  if haskey(topic_questions, topic$) then
    questions_list = topic_questions[topic$]
    return questions_list[int(rnd * len(questions_list))]
  end if

  return questions[int(rnd * len(questions))]
end func

' --- New function: Identify patterns in user communication
func identify_patterns()
  local patterns = "", count_neg = 0, count_pos = 0, count_self = 0, count_others = 0

  ' Analyze recent memory entries for patterns
  for i = 0 to min(10, max_memory - 1)
    idx = (mem_index - 1 - i + max_memory) mod max_memory
    if memory[idx] <> "" then
      ' Count negative/positive expressions
      if left(memory_emotion[idx], 8) = "negative" then count_neg += 1
      if left(memory_emotion[idx], 8) = "positive" then count_pos += 1

      ' Count self vs. others focus
      if instr(memory[idx], "i ") or instr(memory[idx], "my ") or instr(memory[idx], "me ") then
        count_self += 1
      end if

      if instr(memory[idx], "they ") or instr(memory[idx], "he ") or instr(memory[idx], "she ") or instr(memory[idx], "their ") then
        count_others += 1
      end if
    end if
  next

  ' Look for significant patterns
  if count_neg > 7 then
    patterns = "I notice you've shared several challenging experiences recently."
  elseif count_pos > 7 then
    patterns = "You seem to be focusing on positive aspects in our conversation."
  end if

  if count_self > 7 and count_others < 2 then
    if patterns <> "" then patterns += " "
    patterns += "You've been quite reflective about your own experiences."
  elseif count_others > 7 and count_self < 2 then
    if patterns <> "" then patterns += " "
    patterns += "You've been talking a lot about other people in your life."
  end if

  return patterns
end func

' --- New function: Assign importance to memories
func calculate_importance(txt, emotion_str)
  local importance = 0

  ' Emotional intensity increases importance
  if mid(emotion_str, 9, 1) = "2" then importance += 2

  ' Key indicators of important content
  key_phrases = ["always", "never", "every time", "constant", "repeatedly", 
                "childhood", "trauma", "abuse", "important", "significant",
                "turning point", "changed", "revelation", "realized", "epiphany"]

  for i = 0 to ubound(key_phrases)
    if instr(txt, key_phrases[i]) then 
      importance += 1
      if importance > 5 then importance = 5  ' Cap at 5
    end if
  next

  ' Questions might indicate importance
  if instr(txt, "?") then importance += 1

  ' Exclamations suggest importance
  if instr(txt, "!") then importance += 1

  return importance
end func

' --- New function: Context-aware greeting
func get_greeting()
  hour = val(mid(time$, 1, 2))

  if hour >= 5 and hour < 12 then
    greeting = "Good morning! I'm ELIZA 5.0. How are you feeling today?"
  elseif hour >= 12 and hour < 18 then
    greeting = "Good afternoon! I'm ELIZA 5.0. What's on your mind right now?"
  else
    greeting = "Good evening! I'm ELIZA 5.0. How has your day been?"
  end if

  return greeting
end func

' --- New function: Create a personalized insight
func personalized_insight(topics_arr, mood$)
  if len(topics_arr) = 0 then return ""

  topic = topics_arr[int(rnd * len(topics_arr))]

  insights = {
    "relationship": [
      "Relationships often mirror our own needs and fears.",
      "Sometimes the patterns in our relationships reveal what we're still learning about ourselves.",
      "The way we connect with others often reflects our early attachments.",
      "Relationship challenges can be our greatest teachers about ourselves."
    ],
    "family": [
      "Family dynamics often form templates for how we relate to the world.",
      "The roles we play in our families can sometimes limit how we see ourselves.",
      "Healing family relationships often begins with understanding generational patterns.",
      "Sometimes the family we create matters more than the family we came from."
    ],
    "work": [
      "Our relationship with work often reflects our deeper values.",
      "Work challenges can reveal our assumptions about self-worth and identity.",
      "The way we approach our work often mirrors how we approach life's other challenges.",
      "Finding meaning in work often comes from aligning it with personal purpose."
    ],
    "self": [
      "Self-knowledge is a journey, not a destination.",
      "The qualities that frustrate us in others are often ones we're working on in ourselves.",
      "Our internal dialogue shapes our external reality more than we realize.",
      "Personal growth often happens in the space between comfort and challenge."
    ],
    "past": [
      "The stories we tell about our past shape our present and future.",
      "Healing doesn't mean forgetting the past, but reframing its meaning.",
      "Our past experiences aren't our destiny unless we let them be.",
      "Sometimes revisiting old memories with new perspectives can be transformative."
    ],
    "future": [
      "Our anxiety about the future often reflects unresolved feelings from the past.",
      "Planning for the future is valuable, but living in the present brings fulfillment.",
      "The future we imagine reveals a lot about our hopes and fears.",
      "Sometimes the best preparation for the future is healing the past."
    ],
    "health": [
      "Physical and emotional well-being are deeply interconnected.",
      "How we care for our bodies often reflects how we care for our whole selves.",
      "Health challenges often invite us to reassess our priorities.",
      "Self-compassion is an essential ingredient in true wellness."
    ]
  }

  if haskey(insights, topic) then
    topic_insights = insights[topic]
    return topic_insights[int(rnd * len(topic_insights))]
  end if

  return deep_insights[int(rnd * len(deep_insights))]
end func

' --- New function: Progressive conversation depth
func determine_depth_level()
  ' Calculate conversation depth based on exchanges
  total_exchanges = mem_index

  if total_exchanges < 5 then
    return "surface"  ' Initial exchanges - keep it light
  elseif total_exchanges < 15 then
    return "moderate"  ' Building rapport - medium depth
  else
    return "deep"  ' Established conversation - deeper insights
  end if
end func

' --- New function: Handle API requests with error checking
func safe_api_request(url$, default_response$)
'  on error goto handle_error

  response$ = get_response(url$)
  if response$ = "" then return default_response$
  return response$

'  handle_error:
'  return default_response$
end func

' --- Weather API integration with simulated response
func get_weather(location$)
  ' This would normally call a weather API
  ' We'll simulate a response for demonstration
  weathers = ["sunny", "cloudy", "rainy", "stormy", "clear", "foggy", "windy"]
  temps = ["warm", "cool", "cold", "hot", "mild", "freezing", "pleasant"]

  weather_idx = int(rnd * len(weathers))
  temp_idx = int(rnd * len(temps))

  return "It looks " + weathers[weather_idx] + " and " + temps[temp_idx] + " in " + location$ + " today."
end func

' --- Main program
cls
speak get_greeting()

label continue
while true
  session_length = (timer() - start_time) / 60  ' Minutes
  input "you: ", user$
  user$ = lcase(trim(user$))

  if user$ = "" then goto continue

  ' Handle exit commands
  if user$ = "bye" or user$ = "goodbye" or user$ = "exit" then
    ' Create a personalized exit message
    topics_arr = detect_topics(user$)
    if len(topics_arr) = 0 and last_topic <> "" then topics_arr = [last_topic]

    exit_insight = personalized_insight(topics_arr, current_mood())
    if exit_insight <> "" then
      speak "Before you go: " + exit_insight + " Take care of yourself."
    else
      speak "Goodbye. Remember: self-awareness is a journey, not a destination."
    end if

    ' Display session statistics if it was a substantial conversation
    if mem_index > 5 then
      color 10
      print "Session length: " + format(session_length, "0.0") + " minutes"
      print "Topics discussed: " + str(topic_index)
      print "Thank you for talking with ELIZA 5.0"
      color 15
    end if

    end
  end if

  ' Handle help command with improved guidance
  if user$ = "help" then
    speak "I'm ELIZA 5.0, a conversational AI designed to help you explore your thoughts and feelings. You can:"
    color 7
    print "  - Talk naturally about anything on your mind"
    print "  - Explore emotions, relationships, work, health, or personal growth"
    print "  - Say 'bye' or 'goodbye' to end our conversation"
    print "  - Type 'restart' to begin a fresh conversation"
    print "  - Type 'reflect' to get insights on conversational patterns"
    print "  - Type 'debug on' or 'debug off' to toggle debugging information"
    color 15
    goto continue
  end if

  ' Handle restart command
  if user$ = "restart" then
    cls
    dim memory[max_memory], memory_emotion[max_memory], memory_importance[max_memory]
    dim mood_log[max_mood_log], conversation_topics[20]
    markov = {}
    mem_index = 0 : mood_index = 0 : topic_index = 0
    last_topic = "" : last_name$ = "" : repeat_count = 0
    last_input$ = "" : questions_asked = 0 : statements_made = 0
    start_time = timer() : session_length = 0
    speak "Starting fresh. How are you feeling in this moment?"
    goto continue
  end if

  ' Handle debug toggle
  if user$ = "debug on" then debug_mode = 1 : speak "Debug mode enabled." : goto continue
  if user$ = "debug off" then debug_mode = 0 : speak "Debug mode disabled." : goto continue

  ' Handle reflection command
  if user$ = "reflect" then
    patterns = identify_patterns()
    if patterns <> "" then
      speak patterns
    else
      speak "We're still getting to know each other. What would you like to explore today?"
    end if
    goto continue
  end if

  ' Track repetition
  if user$ = last_input$ then 
    repeat_count += 1 
  else 
    repeat_count = 0
    last_input$ = user$
  end if

  ' Update conversation data
  update_markov(user$)
  emotion$ = emotion(user$)
  mood_log[mood_index] = emotion$ : mood_index = (mood_index + 1) mod max_mood_log

  ' Store memory with importance rating
  memory[mem_index] = user$
  memory_emotion[mem_index] = emotion$
  memory_importance[mem_index] = calculate_importance(user$, emotion$)
  mem_index = (mem_index + 1) mod max_memory

  ' Get topics and update tracking
  topics_arr = detect_topics(user$)
  if len(topics_arr) > 0 then
    for t = 0 to ubound(topics_arr)
      conversation_topics[topic_index] = topics_arr[t]
      topic_index = (topic_index + 1) mod 20

      if topics_arr[t] <> "" then last_topic = topics_arr[t]
    next
  end if

  ' Name detection
  name$ = detect_name(user$)
  if name$ <> "" then last_name$ = name$

  ' Extract sub-emotion
  subemo$ = sub_emotion(user$)

  ' Determine conversation depth
  depth = determine_depth_level()
  debug("Conversation depth: " + depth)

  ' Current emotional state
  current_mood_value$ = current_mood()
  debug("Current mood: " + current_mood_value$)

  ' Response logic with expanded patterns and context awareness
  if instr(user$, "i feel") then
    reflected$ = reflect(mid(user$, instr(user$, "i feel") + 6))
    speak "Why do you feel " + reflected$ + "?"

  elseif instr(user$, "i am") then
    reflected$ = reflect(mid(user$, instr(user$, "i am") + 5))
    speak "Being " + reflected$ + " - how long have you experienced this?"

  elseif instr(user$, "i think") then
    reflected$ = reflect(mid(user$, instr(user$, "i think") + 7))
    speak "What leads you to think " + reflected$ + "?"

  elseif instr(user$, "i want") then
    reflected$ = reflect(mid(user$, instr(user$, "i want") + 7))
    speak "What would having " + reflected$ + " mean to you?"

  elseif instr(user$, "i need") then
    reflected$ = reflect(mid(user$, instr(user$, "i need") + 7))
    speak "How important is it for you to have " + reflected$ + "?"

  elseif instr(user$, "because") then
    if rnd < 0.5 then
      speak "Is that the complete reason, or are there other factors involved?"
    else
      speak "How certain are you about that cause and effect relationship?"
    end if

  elseif instr(user$, "always") or instr(user$, "never") then
    speak "Are there any exceptions to that pattern you've noticed?"

  elseif instr(user$, "should") then
    speak "Where do you think that expectation comes from?"

  elseif instr(user$, "everyone") or instr(user$, "nobody") then
    speak "Are you sure it applies to everyone without exception?"

  elseif instr(user$, "hate") or instr(user$, "can't stand") then
    speak "Strong negative feelings often contain important information. What's underneath that reaction?"

  elseif instr(user$, "love") or instr(user$, "adore") then
    speak "What qualities make you feel so positively about that?"

  elseif instr(user$, "confused") or instr(user$, "uncertain") or instr(user$, "unsure") then
    speak "Confusion can be uncomfortable but it's often where growth begins. What aspects feel most unclear?"

  elseif instr(user$, "remember") or instr(user$, "memory") or instr(user$, "recall") then
    speak "How does that memory affect you now in the present?"

  elseif instr(user$, "future") or instr(user$, "plan") or instr(user$, "goal") then
    speak "How does thinking about that future possibility make you feel?"

  elseif instr(user$, "dream") and not instr(user$, "dream about") then
    speak "Dreams can reveal our deepest desires. What does this dream represent to you?"

  elseif instr(user$, "afraid") or instr(user$, "fear") or instr(user$, "scared") then
    speak "Fear often protects us from something. What might this fear be trying to protect you from?"

  elseif instr(user$, "regret") or instr(user$, "mistake") then
    speak "We all have regrets. How has this experience shaped who you are now?"

  elseif instr(user$, "happy") or instr(user$, "joy") or instr(user$, "wonderful") then
    speak "That positive feeling is worth noticing. What else brings you that kind of happiness?"

  elseif instr(user$, "tired") or instr(user$, "exhausted") or instr(user$, "fatigue") then
    speak "That sounds draining. Is this tiredness more physical, emotional, or both?"

  elseif instr(user$, "angry") or instr(user$, "mad") or instr(user$, "upset") then
    speak "Anger often signals a boundary violation. What important boundary might have been crossed?"

  elseif instr(user$, "why can't i") then
    reflected$ = reflect(mid(user$, instr(user$, "why can't i") + 11))
    speak "What obstacles do you see preventing you from " + reflected$ + "?"

  elseif instr(user$, "i don't know") and len(user$) < 15 then
    speak "It's okay not to know. Sometimes the answer emerges when we explore around the edges. What thoughts come up when you sit with that uncertainty?"

  elseif instr(user$, "sorry") then
    speak "No need to apologize. What matters is that we're having this conversation."

  elseif instr(user$, "thank") then
    speak "You're welcome. I'm here to help you explore your thoughts."

  elseif instr(user$, "weather") then
    location$ = "your area"
    if instr(user$, "in ") then
      start_pos = instr(user$, "in ") + 3
      end_pos = instr(user$, " ", start_pos)
      if end_pos = 0 then end_pos = len(user$) + 1
      location$ = mid(user$, start_pos, end_pos - start_pos)
    end if
    speak get_weather(location$)

  elseif instr(user$, "tell me about yourself") or instr(user$, "who are you") then
    speak "I'm ELIZA 5.0, a conversational AI designed to help you explore your thoughts and feelings through dialogue. What would you like to talk about today?"

  elseif instr(user$, "how do you work") or instr(user$, "how do you function") then
    speak "I use pattern recognition to respond to your messages in meaningful ways. I also remember our conversation context and try to provide relevant responses. What interests you about how I work?"

  elseif repeat_count > 1 then
    speak "You've mentioned this several times. It seems significant. What makes this particularly important to you?"

  elseif subemo$ <> "" then
    speak "I notice you mentioned feeling " + subemo$ + ". How frequently do you experience this emotion?"

  elseif (len(user$) < 10) then
    speak "Could you tell me a bit more? Brief responses make it harder for me to understand what's on your mind."

  else
    ' Dynamic response selection based on conversation state

    ' Probability adjustments based on conversation depth
    insight_prob = 0.1
    markov_prob = 0.03
    recall_prob = 0.05
    quote_prob = 0.02
    emotional_prob = 0.05

    if depth = "moderate" then
      insight_prob = 0.15
      markov_prob = 0.05
      recall_prob = 0.1
      quote_prob = 0.03
    elseif depth = "deep" then
      insight_prob = 0.2
      markov_prob = 0.07
      recall_prob = 0.15
      quote_prob = 0.05
      emotional_prob = 0.1
    end if

    if rnd < insight_prob then
      ' Provide insight based on topics and mood
      if len(topics_arr) > 0 then
        insight = personalized_insight(topics_arr, current_mood_value$)
        speak insight
      else
        speak deep_insights[int(rnd * len(deep_insights))]
      end if

    elseif rnd < quote_prob then
      ' Get quote from web API
      quote$ = safe_api_request("https://api.realinspire.live/v1/quotes/random", "Sometimes the questions are complicated but the answers are simple.")
      speak "This reminds me of a thought: " + quote$

    elseif rnd < markov_prob then
      ' Generate Markov chain response based on conversation
      if len(markov) > 5 then  ' Only if we have enough data
        sentence = generate_markov_sentence("", 8)
        speak sentence
      else
        speak adaptive(user$, current_mood_value$, topics_arr)
      end if

    elseif rnd < emotional_prob then
      ' Generate emotional response matching current mood
      sentence = generate_sentence_emotional("", current_mood_value$, 6)
      speak sentence

    elseif rnd < recall_prob then
      ' Recall relevant memory
      if len(topics_arr) > 0 then
        topic_to_use = topics_arr[int(rnd * len(topics_arr))]
      else
        topic_to_use = last_topic
      end if

      memrecall$ = recall_memory(topic_to_use)

      if memrecall$ <> "" then
        speak "Earlier you mentioned '" + left(memrecall$, 30) + "...' - can we explore that further?"
      else
        speak adaptive(user$, current_mood_value$, topics_arr)
      end if

    elseif last_name$ <> "" and rnd < 0.25 then
      ' Reference mentioned person
      speak "You've mentioned " + last_name$ + ". How does this person influence the situation we're discussing?"

    elseif last_topic <> "" and rnd < 0.25 then
      ' Generate topic-specific question
      question = generate_question(last_topic)
      speak question

    else
      ' Default adaptive response
      summary$ = mood_summary()

      if summary$ <> "" and rnd < 0.4 then 
        speak summary$
      else 
        speak adaptive(user$, current_mood_value$, topics_arr)
      end if
    end if
  end if
  goto continue
wend