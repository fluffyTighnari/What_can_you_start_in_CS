简化基本数据库：因反复调试无果决定暂时冻结，保持原有架构



基本架构：main.cpp -> parse.cpp -> executor.cpp -> storage.cpp -> Btree.cpp （页管理层pager.cpp）

main.cpp 仅支持基本命令，谓词仅支持主键单个等值



main.cpp:调用解析与执行接口

create table table\_name (col1 type1,col2 type2...); type仅支持：int,integer,text,varchar,float 默认col1为主键

delete from table\_name where ...;where可选

insert into table\_name (col1,col2,...) values(val1,val2,...); 要求col1为主键

insert into table\_name values(val1,val2,...); 要求与表属性一一对应

select \* from table\_name where...;where可选

select col1,col2,... from table\_name where...;where可选



parse.cpp:词法解析->语法解析与语义检查（耦合）

tokenize函数（字符串流解析） -> parse\_command系列函数（根据不同主命令规则分流） -> valid检测系列函数（表名检测/列检测/类型检测）



executor.cpp:根据主命令调用storage层执行函数（考虑重构逻辑，将执行函数移动至executor层）



storage.cpp:主要任务：为parse.cpp提供语义检查接口、为executor.cpp提供执行接口、提供序列化/反序列化接口

语义检查逻辑：读取系统表，分析表名状态，将相关数据存入RowHeader -> 分析列/类型状态

执行逻辑：根据解析出的command，进行信息收集/命令类型分类/序列化与反序列化 -> 调用Btree层接口

序列化/反序列化逻辑：利用存储结构进行拼接与分割

/\*B+tree存储结构：

\[RowHeader]\[int:...]\[size]\[string:...]\[RowHeader]\[...]\[...]

|offset\[0]                            |offset\[1]

System\_page存储结构：

\[RowHeader]\[int:page\_num]\[int:column\_num]\[int:size1]\[string:column]\[int:size2]\[string:column\_type]\*/



Btree.cpp:为storage.cpp提供B+树的插入/删除/查找接口

提供B+树分裂和合并的内部接口



pager.cpp:负责全局的内存管理

pager\_t结构体：提供哈希表负责缓存、提供LRU链表负责缓存加入与淘汰、提供mainpage负责表检验与分配

为外部提供获取表、删除表、写入表、更新表接口

为内部提供循环删除接口，适应delete from table情境





