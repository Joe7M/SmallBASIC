#
# Generate the input file using the following SQL query:
#
# select FDB.body_value, FDCB.comment_body_value, FDB.entity_id
# from d7_field_data_body as FDB
# left outer join d7_comment as C on C.nid = FDB.entity_id
# left outer join d7_field_data_comment_body as FDCB on FDCB.entity_id = C.cid
# where FDB.body_value like '%sbasic reference%'
# order by FDB.body_value
#
# Then export the results in JSON format
#

split trim(command), " ", args
tload args(0), in_str, 1

# cleanup the mysq json
in_str = translate(in_str, "\'", "'")
in_str = translate(in_str, "'entity_id' :", "\"entity_id\":")
in_str = translate(in_str, "'body_value' :", "\"body_value\":")
in_str = translate(in_str, "'comment_body_value' :", "\"comment_body_value\":")
in_map = array(in_str)

if (len(args) == 2 && args(1) == "txt") then
  mk_text_reference(in_map)
else
  mk_help(in_map)
fi

func get_field(row, key)
  local n1, n2, result
  n1 = instr(row, key) + len(key)
  n2 = instr(n1, row, "\\r") - n1
  result = mid(row, n1, n2)
  get_field = result
end

func fix_comments(comments)
  comments = translate(comments, "p. ", "")
  comments = translate(comments, "bq. ", "")
  comments = translate(comments, "bc. ", "")
  comments = translate(comments, "bc.. ", "")
  comments = translate(comments, "\\\"", "\"")
  comments = translate(comments, "&nbsp;", " ")
  comments = translate(comments, "<code>", "--Example--" + chr(10))
  comments = translate(comments, "</code>", chr(10) + "--End Of Example--")
  comments = translate(comments, NL, chr(10))
  fix_comments = comments
end

sub mk_help(byref in_map)
  local in_map_len = len(in_map) - 1
  for i = 0 to in_map_len
    row = in_map(i).body_value
    group = get_field(row, "group=")
    type = get_field(row, "type=")
    keyword = get_field(row, "keyword=")
    syntax = get_field(row, "syntax=")
    brief = get_field(row, "brief=")
    ? group + "," + type + "," + keyword + "," + in_map(i).entity_id + ",\"" + syntax + "\",\"" + brief + "\""
  next i
end

sub mk_text_reference(byref in_map)
  local NL = "\\r\\n"
  local in_map_len = len(in_map) - 1
  local end_block = "<!-- end heading block -->"

  ? "SmallBASIC Language reference"
  ? "See: http://smallbasic.sourceforge.net/?q=node/201"
  ?

  for i = 0 to in_map_len
    row = in_map(i).body_value
    group = get_field(row, "group=")
    type = get_field(row, "type=")
    keyword = get_field(row, "keyword=")
    syntax = get_field(row, "syntax=")
    brief = get_field(row, "brief=")
    ? (i+1) + ". (" + group + ") " + keyword
    ?
    ? syntax
    ?
    ? brief
    ?
    pos = instr(row, end_block) + len(end_block)
    if (pos < len(row)) then
      ? fix_comments(mid(row, pos))
    endif
    comments = in_map(i).comment_body_value
    if (comments != "NULL") then
      ? fix_comments(comments)
    endif
  next i
end
