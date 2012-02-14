<?php

define( 'ORM_LOADED',           TRUE );
define( 'QUERY_STATUS',         0 );
define( 'LAST_INSERT_ID',       1 );
define( 'LAST_INSERT_IDS',      2 );
define( 'NUM_AFFECTED_ROWS',    3 );
define( 'QUERY',                4 );
define( 'DATA',                 5 );
define( 'COUNT',                6 );
define( 'RESOURCE_ID',          7 );
define( 'FIELDS',               0 );
define( 'FIELDS_DETAIL',        1 );
define( 'ACTIVE',               0 );
define( 'RESET',              1 );

class Data{
    private $table_info;
    private $query;
    private $connections;
    
    
    /*
        Constructor function to initialize 
          table name ( required ), 
          table alias ( optional ) and 
          database connection name ( optional )
    */
    public function __construct( $table_name, $table_alias=NULL, $db_con=NULL ){
        $this->table_info['table'] = $table_name;
        if( $table_alias )
            $this->table_info['table_alias'] = $table_alias;
    
        if( $db_con )
            $this->connections['active'] = $db_con;         
        
        $this->table_info['fields'] = array(  );

        $this->switch_db( ACTIVE );     
        $res = db_query( "DESCRIBE {`$table_name`};" );
        $this->switch_db( RESET );
        
        while( $field = db_fetch_array($res) ){
            $field_name = $field['Field'];
            unset( $field['Field'] );
            $this->table_info['fields'][$field_name] = $field;
        }

    }

    /*
        A private function that toggeles between database connections if database connection name is defined
    */
    private function switch_db( $mode = ACTIVE ){
        if( isset($this->connections['active']) && $mode == ACTIVE ){
            $this->connections['previous'] = db_set_active( $this->connections['active'] );
            return ACTIVE;
        }

        if( isset($this->connections['previous']) && $mode == RESET ){
            db_set_active( $this->connections['previous'] );
            unset( $this->connections['previous'] );
            return RESET;
        }
    }

    /*
        This function introduces a group of dynamic function call handlers
            load_by_<field>( arg )
                This class of dynamic function helps load data by defining a single condition.
                    e.g. load_by_email( 'abc@example.com' )  : Return the record where email field has value as 'abc@example.com'
                         load_by_status( 1 )                 : Return the record where status field has value as 1
            get_<type>_query( arg )
                This class of dynamic function helps to return SQL query of given type.
                    e.g. get_select_query(  ) : Return SELECT SQL query
                         get_update_query(  ) : Return UPDATE SQL query
            update_where_<field>( arg )
                This class of dynamic function helps to set a condition and trigger update
                    e.g. update_where_email( 'abc@example.com' )
            remove_where_<field>( arg )
                This class of dynamic function helps to set a condition and trigger delete
    */
    public function __call( $method, $args ){
        $method_parts = explode( '_', $method );
        $action = array_shift( $method_parts );
        
        // Dynamic functions to set a condition and trigger load with a pattern load_by_field_name( value )
        if( $action == 'load' ){
            array_shift( $method_parts );
            $field = implode( '_', $method_parts );
            $this->conditions( array($field => $args[0]) );
            return $this->load(  );
        }
        
        // Dynamic function to get a copy of SQL stetement with a pattern get_type_query(  )
        if( $action == 'get' && $method_parts[1] == 'query' ){
            $query_type = $method_parts[0];
            return $this->sqlize( $query_type );
        }
        
        // Dynamic functions to set a condition and trigger update with a pattern update_for_field( value )
        if( $action == 'update' && $method_parts[0] == 'where' ){
            array_shift( $method_parts );
            $field = implode( '_', $method_parts );
            $this->conditions( array($field => $args[0]) );
            return $this->update(  );
        }
        
        // Dynamic functions to set a condition and trigger remove with a pattern remove_where_field( value )
        if( $action == 'remove' && $method_parts[0] == 'where' ){
            array_shift( $method_parts );
            $field = implode( '_', $method_parts );
            $this->conditions( array($field => $args[0]) );
            return $this->remove(  );
        }

        // Dynamic functions to execute a custom query against a specific output behaviour
        // Allowed paterns are 
        //      ( i)    sql($query, args )                          Returns query status or Resource id
        //      ( ii)   sql_query_status($query, $args )                Returns query status or Resource id
        //      ( iii)  sql_last_insert_id($query, $args )          Returns last insert id
        //      ( iv)   sql_last_insert_ids($query, $args )         Returns array of last insert ids in case of multiple record insertion
        //      ( v)        sql_num_affected_rows($query, $args )       Returns number of effected rows
        //      ( vi)   sql_data($query, $args )                        Returns data set
        if( $action == 'sql' ){
            $type = NULL;
            if( !empty($method_parts) ){
                $type = implode( '_', $method_parts );
                $type = constant( strtoupper($type) );
            }

            switch ( $type ) {
                case LAST_INSERT_ID:
                    $this->switch_db( ACTIVE );
                    $res = call_user_func_array( 'db_query', $args );
                    $last_insert_id = db_last_insert_id( $this->table_info['table'], $this->get_primary_key( ) );                    
                    $this->switch_db( RESET );
                    return $last_insert_id;
                break;
                case LAST_INSERT_IDS:
                    $this->switch_db( ACTIVE );
                    $res = call_user_func_array( 'db_query', $args );
                    $affected_rows = db_affected_rows(  );
                    $last_ids = array(  );
                    if( $affected_rows == 1 ){
                        $last_ids[] = db_last_insert_id( $this->table_info['table'], $this->get_primary_key( ) );
                    }else if( $affected_rows > 1 ){
                        $pk = $this->get_primary_key(  );
                        $last_ids = $this->fields( array($pk) )->sort(array("$pk DESC"))->limit($affected_rows)->load( );
                    }
                    $this->switch_db( RESET );
                    return array_reverse( $last_ids );
                break;
                case NUM_AFFECTED_ROWS:
                    $this->switch_db( ACTIVE );
                    $res = call_user_func_array( 'db_query', $args );
                    $affected_rows = db_affected_rows(  );
                    $this->switch_db( RESET );
                    return $affected_rows;
                break;
                case DATA:
                    $this->switch_db( ACTIVE );
                    $res = call_user_func_array( 'db_query', $args );
                    $results = array(  );
                    while( $rec = db_fetch_array($res) ){
                        $results[] = $rec;
                    }
                    $this->switch_db( RESET );
                    return $results;
                break;
                case QUERY_STATUS:
                default:
                    $this->switch_db( ACTIVE );
                    $res = call_user_func_array( 'db_query', $args );
                    $this->switch_db( RESET );
                    return $res;
                break;                  
            } // End switch statement
        } // End sql action
    } // End __call function

    /*
        Query builder function to list fields in SELECT clause of SQL statement
    */

    public function fields( $fields = array( ) ){
        if( !is_null($fields) && !empty($fields) )
            $this->query['fields'] = $fields;

        return $this;
    }

    /*
        Query builder function to set conditions in WHERE clause of SQL statement
    */
    public function conditions( $conditions = array( ), $limit=NULL, $offset=NULL ){
        if( !is_null($conditions) && !empty($conditions) )
            $this->query['conditions'] = $conditions;

        if( !is_null($limit) )
            $this->query['limit'] = $limit;

        if( !is_null($offset) )
            $this->query['offset'] = $offset;

        return $this;
    }

    /*
        Query builder function to set LIMIT and OFFSET clause in SQL statement
    */
    public function limit( $limit=NULL, $offset=NULL ){
        if( !is_null($limit) )
            $this->query['limit'] = $limit;

        if( !is_null($offset) )
            $this->query['offset'] = $offset;

        return $this;
    }

    /*
        Query builder function to set ORDER BY clause of SQL statement
    */
    public function sort( $sort=array( ) ){
        if( !is_null($sort) && !empty($sort) )
            $this->query['sort'] = $sort;

        return $this;
    }

    /*
        Query builder function to add JOINS in SQL statement, this function supports multiple joins
    */  
    public function joins( $type=NULL, $join = array( ) ){
        if( !is_null($type) && !is_null($join) && !empty($join) )
            $this->query['join'][][$type] = $join;
        
        return $this;
    }

    /*
        Query builder function to list newly insert values in INSERT SQL statement
    */  
    public function values( $values = array( ) ){
        if( !empty($values) )
            $this->query['values'] = $values;
        
        return $this;
    }

    /*
        Query builder function to list fields against which to values in INSERT SQL statement
    */
    public function set( $set ){
        if( !empty($set) )
            $this->query['set'] = $set;
        
        return $this;
    }

    /*
        Query builder function to list changes in UPDATE SQL statement
    */
    public function changes( $changes = array( ) ){
        if( !empty($changes) )
            $this->query['changes'] = $changes;
        
        return $this;
    }

    /*
        Query processing function that covers SELECT query related features
    */
    public function load( $status = DATA ){
        switch( $status ){
            case DATA:
                $sql = $this->sqlize( 'select' );
                $this->switch_db( ACTIVE ); 
                $res = db_query( $sql );    
                $this->query = array(  );
                $result = array(  );
                while( $record = db_fetch_array($res) ){
                    $result[] = $record;
                }
                $this->switch_db( RESET );
                return $result;
            break;
            case RESOURCE_ID:
                $sql = $this->sqlize( 'select' );
                $this->switch_db( ACTIVE );
                $res = db_query( $sql );
                $this->query = array(  );
                $this->switch_db( RESET );
                return $res;
            break;
            case COUNT:
                $count = $this->fields( array(array('COUNT(`' . $this->get_primary_key( ) . '`)', 'count')))->load( );
                return $count[0]['count'];
            break;
        } // End switch statement
    } // End load stetement

    /*
        Query processing function that covers INSERT query related features
    */
    public function save( $status = QUERY_STATUS ){
        switch( $status ){
            case QUERY_STATUS:
                $sql = $this->sqlize( 'insert' );
                $this->query = array(  );
                $this->switch_db( ACTIVE );
                $result = db_query( $sql );
                $this->switch_db( RESET );
                return $result;
            break;
            case LAST_INSERT_ID:
                $sql = $this->sqlize( 'insert' );
                $this->query = array(  );
                $this->switch_db( ACTIVE );
                db_query( $sql );
                $last_insert_id = db_last_insert_id( $this->table_info['table'], $this->get_primary_key( ) );
                $this->switch_db( RESET );
                return $last_insert_id;
            break;
            case LAST_INSERT_IDS:
                $sql = $this->sqlize( 'insert' );
                $this->query = array(  );
                $this->switch_db( ACTIVE );
                db_query( $sql );
                $affected_rows = db_affected_rows(  );
                $last_ids = array(  );
                if( $affected_rows == 1 ){
                    $last_ids[] = db_last_insert_id( $this->table_info['table'], $this->get_primary_key( ) );
                }else if( $affected_rows > 1 ){
                    $pk = $this->get_primary_key(  );
                    $last_ids = $this->fields( array($pk))->sort(array("$pk DESC"))->limit($affected_rows)->load( );
                }
                $this->switch_db( RESET );
                return array_reverse( $last_ids );
            break;
            case NUM_AFFECTED_ROWS:
                $sql = $this->sqlize( 'insert' );
                $this->query = array(  );
                $this->switch_db( ACTIVE );
                db_query( $sql );
                $affected_rows = db_affected_rows(  );
                $this->switch_db( RESET );
                return $affected_rows;
            break;
            
        } // End switch statement
    } // End save function

    /*
        Query processing function that covers UPDATE query related features
    */
    public function update( $status = QUERY_STATUS ){
        switch( $status ){
            case QUERY_STATUS:
                $sql = $this->sqlize( 'update' );
                $this->query = array(  );
                $this->switch_db( ACTIVE ); 
                $result = db_query( $sql );
                $this->switch_db( RESET );    
                return $result;
            break;
            case NUM_AFFECTED_ROWS:
                $sql = $this->sqlize( 'update' );
                $this->query = array(  );
                $this->switch_db( ACTIVE ); 
                db_query( $sql );
                $affected_rows = db_affected_rows(  );
                $this->switch_db( RESET );    
                return $affected_rows;
            break;
        }
    }

    /*
        Query processing function that covers DELETE query related features
    */
    public function remove( $status = QUERY_STATUS ){
        switch( $status ){
            case QUERY_STATUS:
                $sql = $this->sqlize( 'delete' );
                $this->query = array(  );
                $this->switch_db( ACTIVE ); 
                $result = db_query( $sql );
                $this->switch_db( RESET );
                return $result;
            break;
            case NUM_AFFECTED_ROWS:
                $sql = $this->sqlize( 'delete' );
                $this->query = array(  );
                $this->switch_db( ACTIVE ); 
                db_query( $sql );
                $affected_rows = db_affected_rows(  );
                $this->switch_db( RESET );
                return $affected_rows;
            break;
        }
    }

    /*
        Query assembler function that assembles different queries/clauses of 
        SQL statements on the behalf of what query builder function initiated.
    */  
    private function sqlize( $type ){
        $sql = "";
        
        switch( $type ){
            case 'select':
                // Type         
                $sql .= "SELECT ";
                // Fields
                $fields = array(  );
                // -- Join Fields

                if( isset($this->query['join']) ){
                    foreach( $this->query['join'] as $join ){
                        $type = array_keys( $join );
                        $type = $type[0];
                        if( isset($join[$type]['table'][1]) ){
                            foreach( $join[$type]['fields'] as $field ){
                                $fields[] = "`" . $join[$type]['table'][1] . "`.`" . $field . "`" ;
                            }
                        }else{
                            foreach( $join[$type]['fields'] as $field ){
                                $fields[] = "`" . $field . "`" ;
                            }
                        }
                    }
                }
                
                // -- Main Table Fields
                if( !empty($this->query['fields']) && !empty($this->table_info['table_alias']) ){
                    foreach( $this->query['fields'] as $field ){
                        if( is_string($field) )
                            $fields[] = "`" . $this->table_info[table_alias] . "`.`" . $field . "`";
                        if( is_array($field) && count($field) ){
                            if( preg_match('/^[a-zA-Z_]+\(.*\)/', $field[0]) ){
                                $fields[] = $field[0] . " AS `" . $field[1] . "`";
                            }else{
                                $fields[] = "`" . $field[0] . "` AS `" . $field[1] . "`";
                            }
                        }
                    }
                }else if( !empty($this->query['fields']) ){
                    foreach( $this->query['fields'] as $field ){
                        if( is_string($field) )
                            $fields[] = "`" . $field . "`";
                        if( is_array($field) && count($field) )
                            $fields[] = $field[0] . " AS `" . $field[1] . "`";
                    }                           
                }else if(  empty($fields)  ){
                    $sql .= "* ";
                }
                
                if(  !empty($fields)  )
                    $sql .= implode(  ", ", $fields  ) . " ";

                // From
                $sql .= "FROM {`" . $this->table_info[table] . "`} ";
                if(  isset($this->table_info['table_alias'])  )
                    $sql .= $this->table_info['table_alias'] . " ";
                

                // Join
                if(  !empty($this->query['join'])  ){
                    foreach(  $this->query['join'] as $join  ){
                        $type = array_keys(  $join  );
                        $type = $type[0];
                        $sql .= strtoupper(  $type  ) . " JOIN ";   // <type> Join
                        $sql .= "{`" . $join[$type]['table'][0] . "`} "; // Table2 name
                        if(  isset($join[$type]['table'][1])  ){
                            $sql .= "`" . $join[$type]['table'][1] . "` "; // Table2 Alias
                        }

                        $sql .= "ON ";  //ON

                        if(  isset($this->table_info['table_alias'])  )
                            $sql .= "`" . $this->table_info['table_alias'] . "`.";  // Table1 alias
                        $sql .= "`" . $join[$type]['map'][0] . "` = ";                  // Table1 field =
                        
                        if(  isset($join[$type]['table'][1])  ){
                            $sql .= "`" . $join[$type]['table'][1] . "`."; // Table2 Alias // Table2 alias
                        }
                        
                        $sql .= "`" . $join[$type]['map']['1'] . "` ";
                    }
                }
                
                // Filter
                if( !empty($this->query['conditions']) ){
                    $sql .= "WHERE ";
                    $conditions = array(    );
                    foreach(  $this->query['conditions'] as $field => $value  ){
                        if( isset($value) ){
                            if( is_scalar($value) ){
                                $conditions[] = ( (isset($this->table_info['table_alias'])) ? "`".$this->table_info['table_alias']."`."  : "") . "`$field` = " . $this->process_value($value );
                            }else{
                                $conditions[] = ( (isset($this->table_info['table_alias'])) ? "`".$this->table_info['table_alias']."`."  : "") . "`$field` $value[1] " . $this->process_value($value[0] );
                            }
                        }
                    }
                    $sql .= implode( " AND ", $conditions ) . " ";
                }

                //Order by
                if( isset($this->query['sort']) && !empty($this->query['sort']) ){
                    $sql .= "ORDER BY ";
                    if( is_scalar($this->query['sort']) ){
                        $sql .= $this->query['sort'];
                    }else{
                        $orders = array(  );
                        foreach( $this->query['sort'] as $order ){
                            $orders[] = $order;
                        }
                        $sql .= implode( ', ', $orders ) . " ";
                    }
                }

                // Limit
                if( isset($this->query['limit']) )
                    $sql .= "LIMIT " . $this->query[limit] . " ";


                // Offset
                if( isset($this->query['offset']) )
                    $sql .= "OFFSET " . $this->query[offset] .  " ";
                
            break;

            case 'insert':
                $sql = "INSERT INTO ";
                $sql .= "{`" . $this->table_info['table'] . "`} ";
                $sql .= "( ";
                if( !empty($this->query['set']) ){
                    $sql .= "`";
                    $sql .= implode( '`, `', $this->query['set'] );
                    $sql .= "` ";
                }else{
                    $sql .= "`";
                    $sql .= implode( '`, `' , array_keys($this->table_info['fields']) );
                    $sql .= "` ";
                }
                $sql .= ") ";
                $keys = array_keys( $this->query['values'] );
                $insert_records = array(  );
                if( is_array($this->query['values'][$keys[0]]) ){ // Multiple Records
                    foreach( $this->query['values'] as $record_data ){
                    $str_record = array(  );
                    foreach( $record_data as $value ){
                        $str_record[] = $this->process_value( $value );
                    }
                    $insert_records[] = "(  " . implode(', ', $str_record) . "  ) ";
                    }
                }else{ // Singleton Record
                    $str_record = array(  );
                    foreach( $this->query['values'] as $value ){
                    $str_record[] = $this->process_value( $value );
                    }
                    $insert_records[] = "(  " . implode(', ', $str_record) . "  ) ";
                }
                $sql .= "VALUES " . implode( ', ', $insert_records );
            break;

            case 'update';
                $sql = "UPDATE {`" . $this->table_info['table'] . "`} ";
                if( isset($this->table_info['table'][1]) ){
                    $sql .= "`" . $this->table_info['table_alias'] . "` ";
                }
                $sql .= "SET ";
                if( !empty($this->query['changes']) ){
                    $updates = array(  );
                    foreach( $this->query['changes'] as $field => $change ){
                        if( isset($this->table_info['table_alias']) )
                            $updates[] = "`" . $this->table_info['table_alias'] . "`.`" . $field . "` = " . $this->process_value( $change );
                        else
                            $updates[] = "`" . $field . "` = " . $this->process_value( $change );
                    }
                    $sql .= implode( ', ', $updates ) . " ";
                }

                if( !empty($this->query['conditions']) ){
                    $sql .= "WHERE ";
                    $conditions = array(  );
                    foreach( $this->query['conditions'] as $field => $value ){
                        if( isset($value) ){
                            if( is_scalar( $value ) )
                                $conditions[] = ( (isset($this->table_info['table_alias'])) ? "`".$this->table_info['table_alias']."`."  : "") . "`$field` = " . $this->process_value( $value );
                            else
                                $conditions[] = ( (isset($this->table_info['table_alias'])) ? "`".$this->table_info['table_alias']."`."  : "") . "`$field` $value[1] " . $this->process_value($value[0] );
                            
                        }
                    }
                    $sql .= implode( " AND ", $conditions ) . " ";
                }
                
            break;

            case 'delete';
                $sql = "DELETE FROM ";
                $sql .= "`" . $this->table_info['table'] . "` ";

                if( !empty($this->query['conditions']) ){
                    $sql .= "WHERE ";
                    $conditions = array(  );
                    foreach( $this->query['conditions'] as $field => $value ){
                        if( isset($value) ){
                            if( is_scalar($value) )
                                $conditions[] = "`$field` = " . $this->process_value( $value );
                            else
                                $conditions[] = "`$field` $value[1] " . $this->process_value( $value[0] );
                            
                        }
                    }
                    $sql .= implode( " AND ", $conditions ) . " ";
                }
            break;
        }
        return $sql. ";";
    }

    /*
        A private function to escape values for different scenarios
    */
    private function process_value(  $value  ){
        //$temp = $value;
        $value = preg_replace(  '/^%([a-zA-Z0-9]+)/', '%%%${1}', $value  );
        $value = preg_replace(  '/([a-zA-Z0-9]+)%$/', '${1}%%%', $value  );
        //drupal_set_message( $temp . " => " . $value );

        if(  is_int($value)  )
            return $value;
        if(  is_string($value) && preg_match('/^[a-zA-Z_]+\(.*\)$/', $value)  )
            return $value;
        /*if(  is_array($value)  ){
            if( count($value) == 1 && preg_match('/^[a-zA-Z_]+\(.*\)$/', $value[0]) )
                return $value[0];
            else if( count($vlaue) > 1 && preg_match('/^[a-zA-Z_]+\(.*\)$/', $value[0]) )
                $func_name preg_match( '/([a-zA-Z_]+)\(.*\)/', $value[0] );
    
        }*/
        
        if(  is_string($value)  )
            return "'" . db_escape_string( $value ) . "'";
    }

    /*
        A internal function to get field in the table that is primary key and auto_increment as well
    */  
    private function get_primary_key(  ){
        foreach(  $this->table_info['fields'] as $field => $field_info  ){
            if(  $field_info['Key'] == 'PRI' && $field_info['Extra'] == 'auto_increment'  ){
                return $field;
            }
        }
    }
    
    
    /*
        A function to list fields/field details of table
    */
    public function describe(  $scope = FIELDS  ){
        if(  $scope == FIELDS  )
            return array_keys(  $this->table_info['fields']  );
        else if(  $scope == FIELDS_DETAIL  )
            return $this->table_info['fields'];
    }
}
