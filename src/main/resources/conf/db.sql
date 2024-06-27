create table if not exists t_conf
(
    conf_id    bigint primary key auto_increment,
    conf_key   varchar(255) not null,
    conf_value varchar(255) not null
);
insert into t_conf (conf_key, conf_value)
values ('fresh', '1');
create table if not exists t_category
(
    category_id   bigint primary key auto_increment,
    category_name varchar(255) not null
);
insert into t_category (category_name)
values ('default');
create table if not exists t_account
(
    account_id         bigint primary key auto_increment,
    item_name          varchar(255) not null,
    user_name          varchar(255) not null,
    password           varchar(255) not null,
    description        varchar(255),
    note               varchar(255),
    create_time        timestamp,
    last_modified_time timestamp,
    available_status   int,
    category_id        bigint       not null
);