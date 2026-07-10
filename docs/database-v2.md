# Generated database API

`schema.xp` is the only schema source. `xp migrate` parses it into
`SchemaIR`, writes `build/generated/schema.json`, plans migrations from the
latest migration's `schema.json`, and generates `build/generated/db.h`.
Migration SQL is never parsed back into a schema.

Simple operations are generated and typed:

~~~cpp
UserCreate input{.username = "aaryan", .email = "aaryan@example.com"};
auto user = co_await xpd.user.create(input);
auto found = co_await xpd.user.findById(user.id);

UserUpdate update;
update.role = "admin";
co_await xpd.user.updateById(user.id, update);
co_await xpd.user.deleteById(user.id);
~~~

Advanced queries build an expression tree. Only generated columns can become SQL
identifiers, and every value is bound:

~~~cpp
auto users = co_await xpd.user.query()
    .where((UserColumns::role == "admin") && UserColumns::email.endsWith(".org"))
    .orderBy(UserColumns::id.desc())
    .limit(50)
    .all();
~~~

Relations use generated batch loaders:

~~~cpp
UserPostsInclude include;
include.where = PostColumns::title.contains("C++");
include.limitPerParent = 10;
auto users = co_await xpd.user.withPosts(include).findMany();
~~~

Transactions are callback-scoped. Repositories borrow a non-copyable transaction
context; returning commits, while exceptions and early destruction roll back:

~~~cpp
auto user = co_await xpd.transaction([](TransactionClient& tx)
    -> drogon::Task<User> {
    auto created = co_await tx.user.create(UserCreate{...});
    co_return created;
});
~~~

Use `xp migrate deploy` in deployments to apply checked-in, checksummed
migrations. Use `xp db push` only for development schema prototyping; it applies
the current plan without adding migration history. Applied migrations are
checksum-verified and deployment is protected by provider locking where supported.

The deprecated JSON `findUnique` overload exists for alpha source compatibility,
but accepts exactly one generated unique key. Parameterized
`xpd.rawMany<Model>()` is the escape hatch for SQL the typed builder cannot
express.
